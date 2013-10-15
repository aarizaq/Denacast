//
// Copyright (C) 2000 Institut fuer Telematik, Universitaet Karlsruhe
// Copyright (C) 2004,2005 Andras Varga
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
 * @file SimpleUDP.cc
 * @author Jochen Reber
 */

//
// Author: Jochen Reber
// Rewrite: Andras Varga 2004,2005
// Modifications: Stephan Krause
//

#include <omnetpp.h>

#include <CommonMessages_m.h>
#include <GlobalNodeListAccess.h>
#include <GlobalStatisticsAccess.h>

#include <SimpleInfo.h>
#include "InterfaceEntry.h"
#include "UDPPacket.h"
#include "SimpleUDP.h"
#include "IPv4ControlInfo.h"
#include "IPv6ControlInfo.h"

#include "IPvXAddressResolver.h"
#include "InterfaceTableAccess.h"
#include "IPv6InterfaceData.h"
#include "IPv4InterfaceData.h"


#include "IPv4Datagram_m.h"
#include "IPv6Datagram_m.h"


#define EPHEMERAL_PORTRANGE_START 1024
#define EPHEMERAL_PORTRANGE_END   5000


Define_Module( SimpleUDP );

std::string SimpleUDP::delayFaultTypeString;
std::map<std::string, SimpleUDP::delayFaultTypeNum> SimpleUDP::delayFaultTypeMap;

static std::ostream & operator<<(std::ostream & os,
                                 const SimpleUDP::SockDesc& sd)
{
    os << "sockId=" << sd.sockId;
    os << " appGateIndex=" << sd.appGateIndex;
    os << " localPort=" << sd.localPort;
    if (sd.remotePort!=0)
        os << " remotePort=" << sd.remotePort;
    if (!sd.localAddr.isUnspecified())
        os << " localAddr=" << sd.localAddr;
    if (!sd.remoteAddr.isUnspecified())
        os << " remoteAddr=" << sd.remoteAddr;
    if (sd.appGateIndex!=-1)
        os << " appGateIndex=" << sd.appGateIndex;

    return os;
}

static std::ostream & operator<<(std::ostream & os,
                                 const SimpleUDP::SockDescList& list)
{
    for (SimpleUDP::SockDescList::const_iterator i=list.begin();
     i!=list.end(); ++i)
        os << "sockId=" << (*i)->sockId << " ";
    return os;
}

//--------
bool SimpleUDP::addrIsLocal(const IPvXAddress &add)
{
    for (unsigned int i = 0; i < ift->getNumInterfaces(); i++)
    {
        InterfaceEntry * e = ift->getInterface(i);
        if (add.isIPv6())
        {
            if (e->ipv6Data() && e->ipv6Data()->hasAddress(add.get6()))
                return true;
        }
        else
        {
            if (e->ipv4Data() && e->ipv4Data()->getIPAddress() == add.get4())
                return true;
        }
    }
    return false;
}

SimpleUDP::SimpleUDP()
{
    globalStatistics = NULL;
    ift = NULL;
}

SimpleUDP::~SimpleUDP()
{

}

void SimpleUDP::initialize(int stage)
{
    UDP::initialize(stage);
    if(stage == MIN_STAGE_UNDERLAY) {
        WATCH_PTRMAP(socketsByIdMap);
        WATCH_MAP(socketsByPortMap);

        lastEphemeralPort = EPHEMERAL_PORTRANGE_START;
        icmp = NULL;
        icmpv6 = NULL;

        //numSent = 0;
        //numPassedUp = 0;
        //numDroppedWrongPort = 0;
        //numDroppedBadChecksum = 0;
        numQueueLost = 0;
        numPartitionLost = 0;
        numDestUnavailableLost = 0;
        //WATCH(numSent);
        //WATCH(numPassedUp);
        //WATCH(numDroppedWrongPort);
        //WATCH(numDroppedBadChecksum);
        WATCH(numQueueLost);
        WATCH(numPartitionLost);
        WATCH(numDestUnavailableLost);

        globalNodeList = GlobalNodeListAccess().get();
        globalStatistics = GlobalStatisticsAccess().get();
        constantDelay = par("constantDelay");
        useCoordinateBasedDelay = par("useCoordinateBasedDelay");

        delayFaultTypeString = par("delayFaultType").stdstringValue();
        delayFaultTypeMap["live_all"] = delayFaultLiveAll;
        delayFaultTypeMap["live_planetlab"] = delayFaultLivePlanetlab;
        delayFaultTypeMap["simulation"] = delayFaultSimulation;

        switch (delayFaultTypeMap[delayFaultTypeString]) {
        case SimpleUDP::delayFaultLiveAll:
        case SimpleUDP::delayFaultLivePlanetlab:
        case SimpleUDP::delayFaultSimulation:
            faultyDelay = true;
            break;
        default:
            faultyDelay = false;
        }

        jitter = par("jitter");
        nodeEntry = NULL;
        WATCH_PTR(nodeEntry);
    }
}

void SimpleUDP::finish()
{
    globalStatistics->addStdDev("SimpleUDP: Packets sent",
                                numSent);
    globalStatistics->addStdDev("SimpleUDP: Packets dropped with bad checksum",
                                numDroppedBadChecksum);
    globalStatistics->addStdDev("SimpleUDP: Packets dropped due to queue overflows",
                                numQueueLost);
    globalStatistics->addStdDev("SimpleUDP: Packets dropped due to network partitions",
                                numPartitionLost);
    globalStatistics->addStdDev("SimpleUDP: Packets dropped due to unavailable destination",
                                numDestUnavailableLost);
}

void SimpleUDP::updateDisplayString()
{
    char buf[80];
    sprintf(buf, "passed up: %d pks\nsent: %d pks", numPassedUp, numSent);
    if (numDroppedWrongPort>0) {
        sprintf(buf+strlen(buf), "\ndropped (no app): %d pks", numDroppedWrongPort);
        getDisplayString().setTagArg("i",1,"red");
    }
    if (numQueueLost>0) {
        sprintf(buf+strlen(buf), "\nlost (queue overflow): %d pks", numQueueLost);
        getDisplayString().setTagArg("i",1,"red");
    }
    getDisplayString().setTagArg("t",0,buf);
}

void SimpleUDP::processUndeliverablePacket(UDPPacket *udpPacket, cPolymorphic *ctrl)
{
    numDroppedWrongPort++;
    EV << "[SimpleUDP::processUndeliverablePacket()]\n"
       << "    Dropped packet bound to unreserved port, ignoring ICMP error"
       << endl;

    delete udpPacket;
}


void SimpleUDP::processPacketFromApp(cPacket *appData)
{
    UDPSendCommand *ctrl = check_and_cast<UDPSendCommand *>(appData->removeControlInfo());
    SocketsByIdMap::iterator it = socketsByIdMap.find(ctrl->getSockId());
    if (it == socketsByIdMap.end())
        error("send: no socket with sockId=%d", ctrl->getSockId());

    SockDesc *sd = it->second;
    const IPvXAddress& destAddr = ctrl->getDestAddr();
    ushort destPort = ctrl->getDestPort() == -1 ? sd->remotePort : ctrl->getDestPort();

    SimpleInfo* info = dynamic_cast<SimpleInfo*>(globalNodeList->getPeerInfo(destAddr));

    if (myAddr.isUnspecified())
    {
        cModule *mod = getParentModule();
        cModule* parent = mod != NULL ? mod->getParentModule() : NULL;
        do
        {
            cProperties *properties = mod->getProperties();
            if (properties && properties->getAsBool("node"))
                break;
            mod = parent;
            if (mod)
                parent = mod->getParentModule();
        }
        while (mod != NULL);
        if (mod == NULL)
            error(" propiertie node not found");
        myAddr = IPvXAddressResolver().addressOf(mod);
    }

    if (info == NULL) {
        EV << "[SimpleUDP::processPacketFromApp() @ " << myAddr << "]\n"
                << "    No route to host " << destAddr
                << endl;
        delete appData;
        delete ctrl;
        numDestUnavailableLost++;
        return;
    }

    if (destAddr.isUnspecified() || destPort == -1)
        error("send: missing destination address or port when sending over unconnected port");

//    IPvXAddress ip = IPvXAddressResolver().addressOf(node);
//    Speedhack SK
     //cGate* destGate;

    // add header byte length for the skipped IP header
    cPacket auxPacket;

    if (destAddr.isIPv6())
        auxPacket.setByteLength(UDP_HEADER_BYTES + IPv6_HEADER_BYTES + appData->getByteLength());
    else
        auxPacket.setByteLength(UDP_HEADER_BYTES + IP_HEADER_BYTES + appData->getByteLength());

    SimpleNodeEntry* destEntry = info->getEntry();

    // calculate delay
    simtime_t totalDelay = 0;

    if (ift == NULL)
        ift = InterfaceTableAccess().get();

    bool isLocal = addrIsLocal(destAddr);

    if (!isLocal) {
        SimpleNodeEntry::SimpleDelay temp;
        if (faultyDelay) {
            SimpleInfo* thisInfo = static_cast<SimpleInfo*>(globalNodeList->getPeerInfo(myAddr));
            temp = nodeEntry->calcDelay(&auxPacket, *destEntry,
                                        !(thisInfo->getNpsLayer() == 0 ||
                                          info->getNpsLayer() == 0)); //TODO
        } else {
            temp = nodeEntry->calcDelay(&auxPacket, *destEntry);
        }
        if (useCoordinateBasedDelay == false) {
            totalDelay = constantDelay;
        } else if (temp.second == false) {
            EV << "[SimpleUDP::processPacketFromApp() @ " << myAddr << "]\n"
               << "    Send queue full: packet " << appData << " dropped"
               << endl;
            delete ctrl;
            delete appData;
            numQueueLost++;
            return;
        } else {
            totalDelay = temp.first;
        }
    }

    SimpleInfo* thisInfo = dynamic_cast<SimpleInfo*>(globalNodeList->getPeerInfo(myAddr));

    if (!globalNodeList->areNodeTypesConnected(thisInfo->getTypeID(), info->getTypeID())) {
        EV << "[SimpleUDP::processPacketFromApp() @ " << myAddr << "]\n"
                   << "    Partition " << thisInfo->getTypeID() << "->" << info->getTypeID()
                   << " is not connected"
                   << endl;
        delete ctrl;
        delete appData;
        numPartitionLost++;
        return;
    }

    if (jitter) {
        // jitter
        //totalDelay += truncnormal(0, SIMTIME_DBL(totalDelay) * jitter);

        //workaround (bug in truncnormal(): sometimes returns inf)
        double temp = truncnormal(0, SIMTIME_DBL(totalDelay) * jitter);
        while (temp == INFINITY || temp != temp) { // reroll if temp is INF or NaN
            std::cerr << "\n******* SimpleUDP: truncnormal() -> inf !!\n"
                      << std::endl;
            temp = truncnormal(0, SIMTIME_DBL(totalDelay) * jitter);
        }

        totalDelay += temp;
    }


    EV << "[SimpleUDP::processPacketFromApp() @ " << myAddr << "]\n"
       << "    Packet " << appData << " sent with delay = " << totalDelay
       << endl;

    //RECORD_STATS(globalStatistics->addStdDev("SimpleUDP: delay", totalDelay));

    /* main modifications for SimpleUDP end here */

    int interfaceId = -1;
    if (ctrl->getInterfaceId() == -1)
    {
        if (destAddr.isMulticast())

        {
            std::map<IPvXAddress, int>::iterator it = sd->multicastAddrs.find(destAddr);
            interfaceId = (it != sd->multicastAddrs.end() && it->second != -1) ? it->second : sd->multicastOutputInterfaceId;
        }
        sendDown(appData, sd->localAddr, sd->localPort, destAddr, destPort, interfaceId, DEFAULT_MULTICAST_LOOP,sd->ttl,0,totalDelay);
    }
    else
        sendDown(appData, sd->localAddr, sd->localPort, destAddr, destPort, ctrl->getInterfaceId(), DEFAULT_MULTICAST_LOOP,sd->ttl,0,totalDelay);

    delete ctrl; // cannot be deleted earlier, due to destAddr

}

void SimpleUDP::sendDown(cPacket *appData, const IPvXAddress& srcAddr, ushort srcPort, const IPvXAddress& destAddr, ushort destPort,
                    int interfaceId, bool multicastLoop, int ttl, unsigned char tos, simtime_t totalDelay)
{
    if (destAddr.isUnspecified())
        error("send: unspecified destination address");
    if (destPort<=0 || destPort>65535)
        error("send invalid remote port number %d", destPort);


    SimpleInfo* info = dynamic_cast<SimpleInfo*>(globalNodeList->getPeerInfo(destAddr));
    numSent++;

    if (info == NULL) {
        EV << "[SimpleUDP::processMsgFromApp() @ " << destAddr << "]\n"
           << "    No route to host " << destAddr
           << endl;
        delete appData;
        numDestUnavailableLost++;
        return;
    }

    SimpleNodeEntry* destEntry = info->getEntry();

    UDPPacket *udpPacket = createUDPPacket(appData->getName());
    udpPacket->setByteLength(UDP_HEADER_BYTES);
    udpPacket->encapsulate(appData);

    // set source and destination port
    udpPacket->setSourcePort(srcPort);
    udpPacket->setDestinationPort(destPort);

    if (!destAddr.isIPv6())
    {
        // send to IPv4
        EV << "Sending app packet " << appData->getName() << " over IPv4.\n";
        IPv4ControlInfo *ipControlInfo = new IPv4ControlInfo();
        ipControlInfo->setProtocol(IP_PROT_UDP);
        ipControlInfo->setSrcAddr(srcAddr.get4());
        ipControlInfo->setDestAddr(destAddr.get4());
        ipControlInfo->setInterfaceId(interfaceId);
        ipControlInfo->setMulticastLoop(multicastLoop);
        ipControlInfo->setTimeToLive(ttl);
        ipControlInfo->setTypeOfService(tos);
        udpPacket->setControlInfo(ipControlInfo);

        emit(sentPkSignal, udpPacket);
        sendDirect(udpPacket, totalDelay, 0, destEntry->getUdpIPv4Gate());
    }
    else
    {
        // send to IPv6
        EV << "Sending app packet " << appData->getName() << " over IPv6.\n";
        IPv6ControlInfo *ipControlInfo = new IPv6ControlInfo();
        ipControlInfo->setProtocol(IP_PROT_UDP);
        ipControlInfo->setSrcAddr(srcAddr.get6());
        ipControlInfo->setDestAddr(destAddr.get6());
        ipControlInfo->setInterfaceId(interfaceId);
        ipControlInfo->setMulticastLoop(multicastLoop);
        ipControlInfo->setHopLimit(ttl);
        ipControlInfo->setTrafficClass(tos);
        udpPacket->setControlInfo(ipControlInfo);

        emit(sentPkSignal, udpPacket);
        sendDirect(udpPacket, totalDelay, 0, destEntry->getUdpIPv6Gate());
    }
    numSent++;
}

void SimpleUDP::setNodeEntry(SimpleNodeEntry* entry)
{
    nodeEntry = entry;
}
