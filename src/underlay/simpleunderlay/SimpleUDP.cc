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
#include <DenaCastOverlayMessage_m.h>

#include <SimpleInfo.h>
#include "UDPPacket.h"
#include "SimpleUDP.h"
#include "IPv4ControlInfo.h"
#include "IPv6ControlInfo.h"
#include "ICMPAccess.h"
#include "ICMPv6Access.h"
#include "IPvXAddressResolver.h"
#include "UDPPacket_m.h"

// the following is only for ICMP error processing
#include "ICMPMessage_m.h"
#include "ICMPv6Message_m.h"
#include "IPv4Datagram_m.h"
#include "IPv6Datagram_m.h"


#define EPHEMERAL_PORTRANGE_START 1024
#define EPHEMERAL_PORTRANGE_END   5000


Define_Module( SimpleUDP );

std::string SimpleUDP::delayFaultTypeString;
std::map<std::string, SimpleUDP::delayFaultTypeNum> SimpleUDP::delayFaultTypeMap;

static std::ostream & operator<<(std::ostream & os, const UDP::SockDesc& sd)
{
    os << "sockId=" << sd.sockId;
    os << " appGateIndex=" << sd.appGateIndex;
    os << " localPort=" << sd.localPort;
    if (sd.remotePort != -1)
        os << " remotePort=" << sd.remotePort;
    if (!sd.localAddr.isUnspecified())
        os << " localAddr=" << sd.localAddr;
    if (!sd.remoteAddr.isUnspecified())
        os << " remoteAddr=" << sd.remoteAddr;
    if (sd.multicastOutputInterfaceId!=-1)
        os << " interfaceId=" << sd.multicastOutputInterfaceId;

    return os;
}

static std::ostream & operator<<(std::ostream & os, const UDP::SockDescList& list)
{
    for (UDP::SockDescList::const_iterator i=list.begin(); i!=list.end(); ++i)
        os << "sockId=" << (*i)->sockId << " ";
    return os;
}


//--------

SimpleUDP::SimpleUDP()
{
    globalStatistics = NULL;
}

SimpleUDP::~SimpleUDP()
{

}

void SimpleUDP::handleMessage(cMessage *msg)
{
    // received from the network layer

    if (msg->arrivedOn("network_in"))
    {
        if (dynamic_cast<ICMPMessage *>(msg) || dynamic_cast<ICMPv6Message *>(msg))
            processICMPError(PK(msg));
        else
            processUDPPacket(check_and_cast<UDPPacket *>(msg));
    }
    else // received from application layer
    {
        if (msg->getKind()==UDP_C_DATA)
            processPacketFromApp(PK(msg));
        else
            processCommandFromApp(msg);
    }

    if (ev.isGUI())
        updateDisplayString();
}

void SimpleUDP::initialize(int stage)
{

    if(stage == MIN_STAGE_UNDERLAY) {
        UDP::initialize();

        numSent = 0;
        numPassedUp = 0;
        numDroppedWrongPort = 0;
        numDroppedBadChecksum = 0;
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
            break;
        }

        jitter = par("jitter");
        //// Add by DenaCast
        dropErrorPackets = par("dropErrorPackets");
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

void SimpleUDP::bind(int sockId, int gateIndex, const IPvXAddress& localAddr, int localPort)
{
    // XXX checks could be added, of when the bind should be allowed to proceed

    UDP::bind(sockId, gateIndex, localAddr, localPort);
    cModule *node = getParentModule();
    IPvXAddress ip = IPvXAddressResolver().addressOf(node);
    EV << "[SimpleUDP::bind() @ " << ip <<  endl;
}


void SimpleUDP::unbind(int sockId)
{
    // remove from socketsByIdMap
    SocketsByIdMap::iterator it = socketsByIdMap.find(sockId);
    if (it==socketsByIdMap.end())
        error("socket id=%d doesn't exist (already closed?)", sockId);
    SockDesc *sd = it->second;

    EV << "[SimpleUDP::unbind() @ " << sd->localAddr << "]\n"
       << "    Unbinding socket: " << *sd
       << endl;
    UDP::close(sockId);
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


void SimpleUDP::processPacketFromApp(cPacket *appData)
{
    UDPSendCommand *udpCtrl = check_and_cast<UDPSendCommand *>(appData->removeControlInfo());
    cModule *node = getParentModule();
//    IPvXAddress ip = IPvXAddressResolver().addressOf(node);
//    Speedhack SK


    SocketsByIdMap::iterator it = socketsByIdMap.find(udpCtrl->getSockId());
    if (it == socketsByIdMap.end())
        error("send: no socket with sockId=%d", udpCtrl->getSockId());

    SockDesc *sd = it->second;
    IPvXAddress destAddr = udpCtrl->getDestAddr().isUnspecified() ? sd->remoteAddr : udpCtrl->getDestAddr();
    int destPort = udpCtrl->getDestPort() == -1 ? sd->remotePort : udpCtrl->getDestPort();
    if (destAddr.isUnspecified() || destPort == -1)
        error("send: missing destination address or port when sending over unconnected port");


    //cGate* destGate;
    
    UDPPacket *udpPacket = new UDPPacket(appData->getName());

    //
    udpPacket->setByteLength(UDP_HEADER_BYTES + IP_HEADER_BYTES);
    udpPacket->encapsulate(appData);

    // set source and destination port
    udpPacket->setDestinationPort(destPort);

    int interfaceId = -1;
    if (udpCtrl->getInterfaceId() == -1)
    {
        if (destAddr.isMulticast())
        {
            std::map<IPvXAddress, int>::iterator it = sd->multicastAddrs.find(destAddr);
            interfaceId = (it != sd->multicastAddrs.end() && it->second != -1) ? it->second : sd->multicastOutputInterfaceId;
        }    }
    else
        interfaceId = udpCtrl->getInterfaceId();

    delete udpCtrl;

    SimpleInfo* info = dynamic_cast<SimpleInfo*>(globalNodeList->getPeerInfo(destAddr));
    numSent++;

    if (info == NULL) {
        EV << "[SimpleUDP::processMsgFromApp() @ " << IPvXAddressResolver().addressOf(node) << "]\n"
           << "    No route to host " << destAddr
           << endl;
        numDestUnavailableLost++;
        return;
    }

    SimpleNodeEntry* destEntry = info->getEntry();

    // calculate delay
    simtime_t totalDelay = 0;
    if (sd->localAddr != destAddr) {
        SimpleNodeEntry::SimpleDelay temp;
        if (faultyDelay) {
            SimpleInfo* thisInfo = static_cast<SimpleInfo*>(globalNodeList->getPeerInfo(sd->localAddr));
            temp = nodeEntry->calcDelay(udpPacket, *destEntry,
                                        !(thisInfo->getNpsLayer() == 0 ||
                                          info->getNpsLayer() == 0)); //TODO
        } else {
            temp = nodeEntry->calcDelay(udpPacket, *destEntry);
        }
        if (useCoordinateBasedDelay == false) {
            totalDelay = constantDelay;
        } else if (temp.second == false) {
            EV << "[SimpleUDP::processMsgFromApp() @ " << IPvXAddressResolver().addressOf(node) << "]\n"
               << "    Send queue full: packet " << udpPacket << " dropped"
               << endl;
            delete udpPacket;
            numQueueLost++;
            return;
        } else {
            totalDelay = temp.first;
        }
    }

    SimpleInfo* thisInfo = dynamic_cast<SimpleInfo*>(globalNodeList->getPeerInfo(sd->localAddr));

    if (!globalNodeList->areNodeTypesConnected(thisInfo->getTypeID(), info->getTypeID())) {
        EV << "[SimpleUDP::processMsgFromApp() @ " << IPvXAddressResolver().addressOf(node) << "]\n"
                   << "    Partition " << thisInfo->getTypeID() << "->" << info->getTypeID()
                   << " is not connected"
                   << endl;
        delete udpPacket;
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

    if (udpPacket != NULL) {
        BaseOverlayMessage* temp = NULL;

        if (ev.isGUI() && udpPacket->getEncapsulatedPacket()) {
            if ((temp = dynamic_cast<BaseOverlayMessage*>(udpPacket
                    ->getEncapsulatedPacket()))) {
                switch (temp->getStatType()) {
                case APP_DATA_STAT:
                    udpPacket->setKind(1);
                    break;
                case APP_LOOKUP_STAT:
                    udpPacket->setKind(2);
                    break;
                case MAINTENANCE_STAT:
                default:
                    udpPacket->setKind(3);
                    break;
                }
            } else {
                udpPacket->setKind(1);
            }
        }

        EV << "[SimpleUDP::processMsgFromApp() @ " << IPvXAddressResolver().addressOf(node) << "]\n"
           << "    Packet " << udpPacket << " sent with delay = " << totalDelay
           << endl;

        //RECORD_STATS(globalStatistics->addStdDev("SimpleUDP: delay", totalDelay));

        // set source and destination port
        udpPacket->setSourcePort(sd->localPort);
        udpPacket->setDestinationPort(destPort);

        if (!destAddr.isIPv6())
        {
            // send to IPv4
            EV << "Sending app packet " << appData->getName() << " over IPv4.\n";
            IPv4ControlInfo *ipControlInfo = new IPv4ControlInfo();
            ipControlInfo->setProtocol(IP_PROT_UDP);
            ipControlInfo->setSrcAddr(sd->localAddr.get4());
            ipControlInfo->setDestAddr(destAddr.get4());
            ipControlInfo->setInterfaceId(interfaceId);
            ipControlInfo->setTimeToLive(sd->ttl);
            ipControlInfo->setTypeOfService(sd->typeOfService);
            udpPacket->setControlInfo(ipControlInfo);

            emit(sentPkSignal, udpPacket);
            sendDirect(udpPacket, totalDelay, 0, destEntry->getGate());
        }
        else
        {
            // send to IPv6
            EV << "Sending app packet " << appData->getName() << " over IPv6.\n";
            IPv6ControlInfo *ipControlInfo = new IPv6ControlInfo();
            ipControlInfo->setProtocol(IP_PROT_UDP);
            ipControlInfo->setSrcAddr(sd->localAddr.get6());
            ipControlInfo->setDestAddr(destAddr.get6());
            ipControlInfo->setInterfaceId(interfaceId);
            ipControlInfo->setHopLimit(sd->ttl);
            ipControlInfo->setTrafficClass(sd->typeOfService);
            udpPacket->setControlInfo(ipControlInfo);

            emit(sentPkSignal, udpPacket);
            sendDirect(udpPacket, totalDelay, 0, destEntry->getGate());
        }
        numSent++;
    }
}


void SimpleUDP::processUndeliverablePacket(UDPPacket *udpPacket, cPolymorphic *ctrl)
{
    numDroppedWrongPort++;

    // send back ICMP PORT_UNREACHABLE
    if (dynamic_cast<IPv4ControlInfo *>(ctrl)!=NULL) {
/*        if (!icmp)
            icmp = ICMPAccess().get();
        IPControlInfo *ctrl4 = (IPControlInfo *)ctrl;
        if (!ctrl4->getDestAddr().isMulticast())
            icmp->sendErrorMessage(udpPacket, ctrl4, ICMP_DESTINATION_UNREACHABLE, ICMP_DU_PORT_UNREACHABLE);*/
        /* TODO: implement icmp module */
        EV << "[SimpleUDP::processUndeliverablePacket()]\n"
                << "    Dropped packet bound to unreserved port, ignoring ICMP error"
                << endl;
    } else if (dynamic_cast<IPv6ControlInfo *>(udpPacket->getControlInfo())
               !=NULL) {
/*        if (!icmpv6)
            icmpv6 = ICMPv6Access().get();
        IPv6ControlInfo *ctrl6 = (IPv6ControlInfo *)ctrl;
        if (!ctrl6->getDestAddr().isMulticast())
            icmpv6->sendErrorMessage(udpPacket, ctrl6, ICMPv6_DESTINATION_UNREACHABLE, PORT_UNREACHABLE);*/
        /* TODO: implement icmp module */
        EV << "[SimpleUDP::processUndeliverablePacket()]\n"
           << "    Dropped packet bound to unreserved port, ignoring ICMP error"
           << endl;
    } else {
        error("(%s)%s arrived from lower layer without control info", udpPacket->getClassName(), udpPacket->getName());
    }
    delete udpPacket;
}


void SimpleUDP::processUDPPacket(UDPPacket *udpPacket)
{
    cModule *node = getParentModule();
//    IPvXAddress ip = IPvXAddressResolver().addressOf(node);
//    Speedhack SK

    // simulate checksum: discard packet if it has bit error
    EV << "[SimpleUDP::processUDPPacket() @ " << IPvXAddressResolver().addressOf(node) << "]\n"
       << "    Packet " << udpPacket->getName() << " received from network, dest port " << udpPacket->getDestinationPort()
       << endl;

    if (udpPacket->hasBitError() && dropErrorPackets) {
        EV << "[SimpleUDP::processUDPPacket() @ " << IPvXAddressResolver().addressOf(node) << "]\n"
           << "    Packet has bit error, discarding"
           << endl;
        delete udpPacket;
        numDroppedBadChecksum++;
        return;
    }
    else   /// add by denacast
    {
    	BaseOverlayMessage* encap = check_and_cast<BaseOverlayMessage*> (udpPacket->decapsulate());
    	if(dynamic_cast<EncapVideoMessage*>(encap) !=NULL)
    		encap->setBitError(udpPacket->hasBitError());
    	udpPacket->encapsulate(encap);
    }


    int destPort = udpPacket->getDestinationPort();
    int srcPort =  udpPacket->getSourcePort();
    cPolymorphic *ctrl = udpPacket->removeControlInfo();

    // send back ICMP error if no socket is bound to that port
    SocketsByPortMap::iterator it = socketsByPortMap.find(destPort);
    if (it==socketsByPortMap.end()) {
        EV << "[SimpleUDP::processUDPPacket() @ " << IPvXAddressResolver().addressOf(node) << "]\n"
           << "    No socket registered on port " << destPort
           << endl;
        processUndeliverablePacket(udpPacket, ctrl);
        delete ctrl;
        return;
    }
    SockDescList& list = it->second;

    int matches = 0;



    // deliver a copy of the packet to each matching socket
    //    cMessage *payload = udpPacket->getEncapsulatedPacket();
    cPacket *payload = udpPacket->getEncapsulatedPacket();
    if (dynamic_cast<IPv4ControlInfo *>(ctrl)!=NULL) {
        IPv4ControlInfo *ctrl4 = (IPv4ControlInfo *)ctrl;
        for (SockDescList::iterator it=list.begin(); it!=list.end(); ++it) {
            SockDesc *sd = *it;
            if (sd->onlyLocalPortIsSet || matchesSocket(sd, udpPacket, ctrl4)) {
//              EV << "[SimpleUDP::processUDPPacket() @ " << IPvXAddressResolver().addressOf(node) << "]\n"
//                 << "    Socket sockId=" << sd->sockId << " matches, sending up a copy"
//                 << endl;
//              sendUp((cMessage*)payload->dup(), udpPacket, ctrl4, sd);
//              ib: speed hack

                if (matches == 0) {
                    sendUp(payload->dup(), sd, ctrl4->getSrcAddr(), srcPort, ctrl4->getDestAddr(), destPort, ctrl4->getInterfaceId(), ctrl4->getTimeToLive(), ctrl4->getTypeOfService());

                } else
                    opp_error("Edit SimpleUDP.cc to support multibinding.");
                matches++;
            }
        }
    } else if (dynamic_cast<IPv6ControlInfo *>(udpPacket->getControlInfo())
               !=NULL) {
        IPv6ControlInfo *ctrl6 = (IPv6ControlInfo *)ctrl;
        for (SockDescList::iterator it=list.begin(); it!=list.end(); ++it) {
            SockDesc *sd = *it;
            if (sd->onlyLocalPortIsSet || matchesSocket(sd, udpPacket, ctrl6)) {
                EV << "[SimpleUDP::processUDPPacket() @ " << IPvXAddressResolver().addressOf(node) << "]\n"
                   << "    Socket sockId=" << sd->sockId << " matches, sending up a copy"
                   << endl;
                sendUp(payload->dup(), sd, ctrl6->getSrcAddr(), srcPort, ctrl6->getDestAddr(), destPort, ctrl6->getInterfaceId(), ctrl6->getHopLimit(), ctrl6->getTrafficClass());
                matches++;
            }
        }
    } else {
        error("(%s)%s arrived from lower layer without control info", udpPacket->getClassName(), udpPacket->getName());
    }

    // send back ICMP error if there is no matching socket
    if (matches==0) {
        EV << "[SimpleUDP::processUDPPacket() @ " << IPvXAddressResolver().addressOf(node) << "]\n"
           << "    None of the sockets on port " << destPort << " matches the packet"
           << endl;
        processUndeliverablePacket(udpPacket, ctrl);
        delete ctrl;
        return;
    }
    delete udpPacket;
    delete ctrl;
}



void SimpleUDP::setNodeEntry(SimpleNodeEntry* entry)
{
    nodeEntry = entry;
}


bool SimpleUDP::matchesSocket(SockDesc *sd, UDPPacket *udp, IPv4ControlInfo *ipCtrl)
{
    // IPv4 version
    if (sd->remotePort!=0 && sd->remotePort!=udp->getSourcePort())
        return false;
    if (!sd->localAddr.isUnspecified() && sd->localAddr.get4()!=ipCtrl->getDestAddr())
        return false;
    if (!sd->remoteAddr.isUnspecified() && sd->remoteAddr.get4()!=ipCtrl->getSrcAddr())
        return false;
    if (sd->multicastOutputInterfaceId !=-1 && sd->multicastOutputInterfaceId != ipCtrl->getInterfaceId())
        return false;
    return true;
}

bool SimpleUDP::matchesSocket(SockDesc *sd, UDPPacket *udp, IPv6ControlInfo *ipCtrl)
{
    // IPv6 version
    if (sd->remotePort!=0 && sd->remotePort!=udp->getSourcePort())
        return false;
    if (!sd->localAddr.isUnspecified() && sd->localAddr.get6()!=ipCtrl->getDestAddr())
        return false;
    if (!sd->remoteAddr.isUnspecified() && sd->remoteAddr.get6()!=ipCtrl->getSrcAddr())
        return false;
    if (sd->multicastOutputInterfaceId !=-1 && sd->multicastOutputInterfaceId != ipCtrl->getInterfaceId())
        return false;
    return true;
}

bool SimpleUDP::matchesSocket(SockDesc *sd, const IPvXAddress& localAddr, const IPvXAddress& remoteAddr, short remotePort)
{
    return (sd->remotePort==0 || sd->remotePort!=remotePort) &&
           (sd->localAddr.isUnspecified() || sd->localAddr==localAddr) &&
           (sd->remoteAddr.isUnspecified() || sd->remoteAddr==remoteAddr);
}

