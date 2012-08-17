//
// Copyright (C) 2006 Institut fuer Telematik, Universitaet Karlsruhe (TH)
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//

/**
 * @file AccessNet.cc
 * @author Markus Mauch
 */

#define _ORIG_INET
#include <vector>
#include <iostream>

#include <omnetpp.h>

#include <IRoutingTable.h>
#include <IInterfaceTable.h>
#include <RoutingTable6.h>
#include <IPvXAddressResolver.h>
#include <IPv4InterfaceData.h>
#include <IPv6InterfaceData.h>

#include "AccessNet.h"

Define_Module(AccessNet);

std::ostream& operator<<(std::ostream& os, NodeInfo& n)
{
    os << n.IPAddress;
    return os;
}

void AccessNet::initialize(int stage)
{
    if(stage != MIN_STAGE_UNDERLAY + 1)
        return;

    router.module = getParentModule();
    router.interfaceTable = IPvXAddressResolver().interfaceTableOf(getParentModule());
    router.routingTable = IPvXAddressResolver().routingTableOf(getParentModule());
    router.IPAddress = IPvXAddressResolver().addressOf(getParentModule());

    channelTypesTx = cStringTokenizer(par("channelTypesTx"), " ").asVector();
    channelTypesRx = cStringTokenizer(par("channelTypesRx"), " ").asVector();
    
//    if (channelTypesRx.size() != channelTypesTx.size()) {
//        channelTypesRx = channelTypesTx;
//    }
    
    int chanIndexTx = intuniform(0, channelTypesTx.size()-1);
    int chanIndexRx = intuniform(0, channelTypesRx.size()-1);

    selectChannel(channelTypesRx[chanIndexRx], channelTypesTx[chanIndexTx]);

    // statistics
    lifetimeVector.setName("Terminal Lifetime");

    WATCH_VECTOR(overlayTerminal);

    lastIP = 0;

    updateDisplayString();
}

void AccessNet::handleMessage(cMessage* msg)
{
    error("this module doesn't handle messages, it runs only in initialize()");
}

int AccessNet::addOverlayNode(cModule* node, bool migrate)
{
    Enter_Method("addOverlayNode()");

    TerminalInfo terminal;
    terminal.module = node;
    terminal.interfaceTable = IPvXAddressResolver().interfaceTableOf(node);
    terminal.remoteInterfaceTable = router.interfaceTable;
    terminal.routingTable = IPvXAddressResolver().routingTableOf(node);
    if (par("wirelessNode").boolValue())
        terminal.WLANInterface = node->getSubmodule("wlan", 0);
    else
        terminal.PPPInterface = node->getSubmodule("ppp", 0);
    terminal.createdAt = simTime();

    // find unassigned ip address:
    //   Start at last given address, check if next address is valid and free.
    bool ip_test = false;
    for (uint32 ipOffset = lastIP + 1; ipOffset != lastIP; ipOffset++) {
        if ( ipOffset == 0x10000) {
            // Netmask = 255.255.0.0, so roll over if offset = 2**16
            ipOffset = 0;
            continue;
        }

        uint32_t ip = router.IPAddress.get4().getInt() + ipOffset;

        // Check if IP is valid:
        //   Reject x.y.z.0 or x.y.z.255 or x.y.255.z
        if ( ((ip & 0xff) == 0) || ((ip & 0xff) == 0xff)
	     || ((ip & 0xff00) == 0xff00) ) {
            continue;
        }

        // Check if IP is free
        ip_test = true;
        for (uint32_t i = 0; i < overlayTerminal.size(); i++) {
            if (overlayTerminal[i].IPAddress == IPv4Address(ip)) {
                ip_test = false;
                break;
            }
        }

        // found valid IP
        if (ip_test) {
            terminal.IPAddress = IPvXAddress(IPv4Address(ip));
            lastIP = ipOffset;
            break;
        }
    }
    if (!ip_test)
        opp_error ("Error creating node: No available IP in access net!");

    // update ip display string
    if (ev.isGUI()) {
        const char* ip_disp = const_cast<char*>
        (terminal.IPAddress.str().c_str());
        terminal.module->getDisplayString().insertTag("t", 0);
        terminal.module->getDisplayString().setTagArg("t", 0, ip_disp);
        terminal.module->getDisplayString().setTagArg("t", 1, "l");
    }


    //
    // Create new remote ppp interface module for this terminal
    //

    // create ppp interface module

    if (!par("wirelessNode").boolValue())
    {
        int k = 1;
        while ( router.module->findSubmodule("ppp", k) != -1 )
            k++;
        cModuleType* pppInterfaceModuleType = cModuleType::get("inet.linklayer.ppp.PPPInterface");
        terminal.remotePPPInterface = pppInterfaceModuleType->
                create("ppp", router.module, 0, k);

    //
    // Connect all gates
    //

    // connect terminal to access router and vice versa
        cGate* routerInGate = firstUnusedGate(router.module, "pppg", cGate::INPUT);
        cGate* routerOutGate = firstUnusedGate(router.module, "pppg", cGate::OUTPUT);

        cChannelType* channelTypeRx = cChannelType::find( channelTypeRxStr.c_str() );
        cChannelType* channelTypeTx = cChannelType::find( channelTypeTxStr.c_str() );
        if (!channelTypeRx || !channelTypeTx)
            opp_error("Could not find Channel or ChannelRx Type. Most likely "
                    "parameter channelTypes does not match the channels defined "
                    "in channels.ned");

        terminal.module->gate("pppg$o", 0)->connectTo(routerInGate,
                channelTypeRx->create(channelTypeRxStr.c_str()));
        routerOutGate->connectTo(terminal.module->gate("pppg$i", 0),
                channelTypeTx->create(channelTypeTxStr.c_str()));

        // connect ppp interface module to router module and vice versa
        routerInGate->connectTo(terminal.remotePPPInterface->gate("phys$i"));
        terminal.remotePPPInterface->gate("phys$o")->connectTo(routerOutGate);

        // connect ppp interface module to network layer module and vice versa
        cModule* netwModule = router.module->getSubmodule("networkLayer");

        cGate* netwInGate = firstUnusedGate(netwModule, "ifIn");
        cGate* netwOutGate = firstUnusedGate(netwModule, "ifOut");

        netwOutGate->connectTo(terminal.remotePPPInterface->gate("netwIn"));
        terminal.remotePPPInterface->gate("netwOut")->connectTo(netwInGate);

    // connect network layer module to ip and arp modules
        cModule* ipModule = router.module->getSubmodule("networkLayer")->
                getSubmodule("ip");

        cGate* ipIn = firstUnusedGate(ipModule, "queueIn");
        netwInGate->connectTo(ipIn);

#ifdef _ORIG_INET
        cModule* arpModule = router.module->getSubmodule("networkLayer")->getSubmodule("arp"); //comment out for speed-hack
        cGate* arpOut = firstUnusedGate(arpModule, "nicOut"); //comment out for speed-hack


        //cGate* ipOut = firstUnusedGate(ipModule, "queueOut"); //comment out for speed-hack
        cGate* ipOut = ipModule->gate("queueOut");

        arpOut->connectTo(netwOutGate);	//comment out for speed-hack
#endif
        terminal.remotePPPInterface->finalizeParameters();
        terminal.remotePPPInterface->setDisplayString("i=block/ifcard");
        terminal.remotePPPInterface->buildInside();
        terminal.remotePPPInterface->scheduleStart(simTime());
        terminal.remotePPPInterface->callInitialize();
        terminal.remoteInterfaceEntry = router.interfaceTable->getInterface(
        router.interfaceTable->getNumInterfaces() - 1);
        terminal.interfaceEntry = terminal.interfaceTable->getInterfaceByName("ppp0");
    }
    else
    {
        terminal.interfaceEntry = terminal.interfaceTable->getInterfaceByName("wlan0");
    }
    //
    // Start ppp interface modules
    //

    if ( !migrate) {
        // we are already in stage 4 and need to call initialize
	// for all previous stages manually
        for (int i=0; i < MAX_STAGE_UNDERLAY + 1; i++) {
            terminal.module->callInitialize(i);
        }
    }


    //
    // Fill in interface table.
    //
    terminal.routingEntry = NULL;
    terminal.remoteRoutingEntry = NULL;
    if (!par("wirelessNode").boolValue())
    {
        // router
        IPv4InterfaceData* interfaceData = new IPv4InterfaceData;
        interfaceData->setIPAddress(router.IPAddress.get4());
        interfaceData->setNetmask(IPv4Address::ALLONES_ADDRESS);
        terminal.remoteInterfaceEntry->setIPv4Data(interfaceData);

    // terminal
        terminal.interfaceEntry->ipv4Data()->setIPAddress(terminal.IPAddress.get4());
        terminal.interfaceEntry->ipv4Data()->setNetmask(IPv4Address::ALLONES_ADDRESS);

    //
    // Fill in routing table.
    //

    // router
        IPv4Route* re = new IPv4Route();
        re->setDestination(terminal.IPAddress.get4());
        re->setNetmask(IPv4Address(IPv4Address::ALLONES_ADDRESS));
        re->setInterface(terminal.remoteInterfaceEntry);
        //re->setType(IPv4Route::DIRECT);
        re->setSource(IPv4Route::MANUAL);
        router.routingTable->addRoute(re);
        terminal.remoteRoutingEntry = re;

    // terminal
        IPv4Route* te = new IPv4Route();
        te->setDestination(IPv4Address::UNSPECIFIED_ADDRESS);
        te->setNetmask(IPv4Address::UNSPECIFIED_ADDRESS);
        te->setGateway(router.IPAddress.get4());
        te->setInterface(terminal.interfaceEntry);
        //te->setType(IPv4Route::REMOTE);
        te->setSource(IPv4Route::MANUAL);
        terminal.routingTable->addRoute(te);
        terminal.routingEntry = te;
    }


    // append module to overlay terminal vector
    overlayTerminal.push_back(terminal);
    int ID = terminal.module->getId();

    updateDisplayString();

    return ID;
}

int AccessNet::getRandomNodeId()
{
    Enter_Method("getRandomNodeId()");

    return overlayTerminal[intuniform(0, overlayTerminal.size() - 1)].module->getId();
}

cModule* AccessNet::removeOverlayNode(int ID)
{
    Enter_Method("removeOverlayNode()");

    cModule* node = NULL;
    TerminalInfo terminal;
    int index;

    for(unsigned int i=0; i<overlayTerminal.size(); i++) {
        if(overlayTerminal[i].module->getId() == ID) {
            terminal = overlayTerminal[i];
            node = terminal.module;
            index = i;
        }
    }

    if(node == NULL) return NULL;

    if (!par("wirelessNode").boolValue())
    {
        cModule* ppp = terminal.remotePPPInterface;

        // disconnect terminal
        node->gate("pppg$o", 0)->disconnect();
        node->gate("pppg$i", 0)->getPreviousGate()->disconnect();

        // disconnect ip and arp modules
        ppp->gate("netwIn")->getPathStartGate()->disconnect();
        ppp->gate("netwOut")->getNextGate()->disconnect();

        // remove associated ppp interface module
        ppp->callFinish();
        ppp->deleteModule();

        // remove associated interface table entry
        router.interfaceTable->deleteInterface(terminal.remoteInterfaceEntry);
    }

    // remove routing entries
    if (terminal.routingEntry)
        terminal.routingTable->deleteRoute(terminal.routingEntry);
    if (terminal.remoteRoutingEntry)
        router.routingTable->deleteRoute(terminal.remoteRoutingEntry);

    // statistics
    lifetimeVector.record(simTime() - overlayTerminal[index].createdAt);

    // remove terminal from overlay terminal vector
    overlayTerminal.erase(overlayTerminal.begin() + index);

    updateDisplayString();

    return node;
}

cModule* AccessNet::getOverlayNode(int ID)
{
    Enter_Method("getOverlayNode()");

    cModule* node = NULL;

    for(unsigned int i=0; i<overlayTerminal.size(); i++) {
        if(overlayTerminal[i].module->getId() == ID)
            node = overlayTerminal[i].module;
    }
    return node;
}

void AccessNet::updateDisplayString()
{
    if (ev.isGUI()) {
        char buf[80];
        if ( overlayTerminal.size() == 1 ) {
            sprintf(buf, "1 terminal connected");
        } else {
            sprintf(buf, "%zi terminals connected", overlayTerminal.size());
        }
        getDisplayString().setTagArg("t", 0, buf);
        getDisplayString().setTagArg("t", 2, "blue");
    }
}

cGate* firstUnusedGate(cModule* owner, const char* name, cGate::Type type)
{
    int index;
    for (index = 0; index < owner->gateSize(name); index++) {
    	cGate *gate = type == cGate::NONE ? owner->gate(name, index) : owner->gateHalf(name, type, index);
        if (!gate->isConnectedOutside()) {
            return gate;
        }
    }

    owner->setGateSize(name, index + 2);
    return type == cGate::NONE ? owner->gate(name, index + 1) : owner->gateHalf(name, type, index + 1);
}

