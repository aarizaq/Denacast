//
// Copyright (C) 2010 DenaCast All Rights Reserved.
// http://www.denacast.com
// The DenaCast was designed and developed at the DML(Digital Media Lab http://dml.ir/)
// under supervision of Dr. Behzad Akbari (http://www.modares.ac.ir/ece/b.akbari)
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/.
//

/**
 * @file SimpleMesh.cc
 * @author Yasser Seyyedi, Behnam Ahmadifar
 */

#include <SimpleInfo.h>
#include "SimpleMesh.h"
#include <GlobalStatistics.h>
#include "DenaCastTrackerMessage_m.h"
#include "VideoMessage_m.h"
#include "SimpleMeshMessage_m.h"


Define_Module(SimpleMesh);

void SimpleMesh::initializeOverlay(int stage)
{
	if (stage != MIN_STAGE_OVERLAY)
		return;
	isRegistered = false;
    if(globalNodeList->getPeerInfo(thisNode.getIp())->getTypeID() == 2)
    	isSource = true;
    else
    	isSource = false;
    passiveNeighbors = par("passiveNeighbors");
    activeNeighbors = par("activeNeighbors");
	neighborNotificationPeriod = par("neighborNotificationPeriod");
	neighborNum = passiveNeighbors + activeNeighbors;

	videoAverageRate = par("videoAverageRate");
	adaptiveNeighboring = par("adaptiveNeighboring");
	serverGradualNeighboring = par("serverGradualNeighboring");
	if(adaptiveNeighboring)
		neighborNum = (int)(upBandwidth/(videoAverageRate*1024) + 1);
	if(isSource)
	{
		neighborNum = activeNeighbors;
		getParentModule()->getParentModule()->setName("CDN-Server");
	}
    DenaCastOverlay::initializeOverlay(stage);
	WATCH(neighborNum);
	WATCH(downBandwidth);
	WATCH(upBandwidth);

	stat_TotalDownBandwidthUsage = 0;
	stat_TotalUpBandwidthUsage = 0;

    stat_joinREQ = 0;
	stat_joinREQBytesSent = 0;
	stat_joinRSP = 0;
	stat_joinRSPBytesSent = 0;
	stat_joinACK = 0;
	stat_joinACKBytesSent = 0;
	stat_joinDNY = 0;
	stat_joinDNYBytesSent = 0;
	stat_disconnectMessages = 0;
	stat_disconnectMessagesBytesSent = 0;
	stat_addedNeighbors = 0;
	stat_nummeshJoinRequestTimer = 0;

}

void SimpleMesh::joinOverlay()
{
	trackerAddress  = *globalNodeList->getRandomAliveNode(1);
	remainNotificationTimer = new cMessage ("remainNotificationTimer");
	scheduleAt(simTime()+neighborNotificationPeriod,remainNotificationTimer);
	std::stringstream ttString;
	ttString << thisNode;
	getParentModule()->getParentModule()->getDisplayString().setTagArg("tt",0,ttString.str().c_str());
	if(!isSource)
	{
		meshJoinRequestTimer = new cMessage("meshJoinRequestTimer");
		scheduleAt(simTime(),meshJoinRequestTimer);
	}
	else
	{
		serverNeighborTimer = new cMessage("serverNeighborTimer");
		if(serverGradualNeighboring)
			scheduleAt(simTime()+uniform(15,25),serverNeighborTimer);
		selfRegister();
	}
	setOverlayReady(true);
}

void SimpleMesh::handleTimerEvent(cMessage* msg)
{
	if(msg == meshJoinRequestTimer)
	{
		if(LV->neighbors.size() < activeNeighbors)
		{
			DenaCastTrackerMessage* NeighborReq = new DenaCastTrackerMessage("NeighborReq");
			NeighborReq->setCommand(NEIGHBOR_REQUEST);
			NeighborReq->setNeighborSize(activeNeighbors - LV->neighbors.size());
			NeighborReq->setSrcNode(thisNode);
			NeighborReq->setIsServer(false);

			sendMessageToUDP(trackerAddress,NeighborReq);
			scheduleAt(simTime()+2,meshJoinRequestTimer);
		}
		else
			scheduleAt(simTime()+20,meshJoinRequestTimer);
	}
	else if(msg == remainNotificationTimer)
	{
		DenaCastTrackerMessage* remainNotification = new DenaCastTrackerMessage("remainNotification");
		remainNotification->setCommand(REMAIN_NEIGHBOR);
		remainNotification->setRemainNeighbor(activeNeighbors - LV->neighbors.size() + passiveNeighbors);
		remainNotification->setSrcNode(thisNode);
		sendMessageToUDP(trackerAddress,remainNotification);
		scheduleAt(simTime()+2,remainNotificationTimer);

	}
	else if (msg == serverNeighborTimer)
	{
		if(isSource)
		{
			if(neighborNum < passiveNeighbors + activeNeighbors)
			{
				neighborNum +=1;
				cancelEvent(remainNotificationTimer);
				scheduleAt(simTime(),remainNotificationTimer);
				scheduleAt(simTime()+uniform(15,25),serverNeighborTimer);
			}
		}
//		else
//		{
//			neighborNum +=1;
//			remainedNeighbor = neighborNum - neighbors.size();
//			cancelEvent(remainNotificationTimer);
//			scheduleAt(simTime(),remainNotificationTimer);
//		}
	}
	else
		DenaCastOverlay::handleAppMessage(msg);
}
void SimpleMesh::handleUDPMessage(BaseOverlayMessage* msg)
{
	if (dynamic_cast<DenaCastTrackerMessage*>(msg) != NULL)
	{
		DenaCastTrackerMessage* trackerMsg = check_and_cast<DenaCastTrackerMessage*>(msg);
		if(trackerMsg->getCommand() == NEIGHBOR_RESPONSE)
		{
			SimpleMeshMessage* joinRequest = new SimpleMeshMessage("joinRequest");
			joinRequest->setCommand(JOIN_REQUEST);
			joinRequest->setSrcNode(thisNode);
			joinRequest->setBitLength(SIMPLEMESHMESSAGE_L(msg));
			int limit = 0;
			if(trackerMsg->getNeighborsArraySize() < activeNeighbors - LV->neighbors.size() )
				limit = trackerMsg->getNeighborsArraySize();
			else
				limit = activeNeighbors - LV->neighbors.size();
			for(unsigned int i=0 ; i <trackerMsg->getNeighborsArraySize() ; i++)
				if(limit-- > 0 && !isInVector(trackerMsg->getNeighbors(i),LV->neighbors))
					sendMessageToUDP(trackerMsg->getNeighbors(i),joinRequest->dup());
			delete joinRequest;
		}
		delete trackerMsg;
	}
	else if (dynamic_cast<SimpleMeshMessage*>(msg) != NULL)
	{
		SimpleMeshMessage* simpleMeshmsg = check_and_cast<SimpleMeshMessage*>(msg);
		if (simpleMeshmsg->getCommand() == JOIN_REQUEST)
		{
			if((int)LV->neighbors.size() < neighborNum)
			{
				LV->neighbors.push_back(simpleMeshmsg->getSrcNode());
				SimpleMeshMessage* joinResponse = new SimpleMeshMessage("joinResponse");
				joinResponse->setCommand(JOIN_RESPONSE);
				joinResponse->setSrcNode(thisNode);
				joinResponse->setBitLength(SIMPLEMESHMESSAGE_L(msg));
				sendMessageToUDP(simpleMeshmsg->getSrcNode(),joinResponse);

				stat_joinRSP += 1;
				stat_joinRSPBytesSent += joinResponse->getByteLength();
			}
			else
			{
				SimpleMeshMessage* joinDeny = new SimpleMeshMessage("joinDeny");
				joinDeny->setCommand(JOIN_DENY);
				joinDeny->setSrcNode(thisNode);
				joinDeny->setBitLength(SIMPLEMESHMESSAGE_L(msg));
				sendMessageToUDP(simpleMeshmsg->getSrcNode(),joinDeny);

				stat_joinDNY += 1;
				stat_joinDNYBytesSent += joinDeny->getByteLength();
			}
		}
		else if (simpleMeshmsg->getCommand() == JOIN_RESPONSE)
		{
			if((int)LV->neighbors.size() < neighborNum )
			{
				LV->neighbors.push_back(simpleMeshmsg->getSrcNode());
				if(!isRegistered)
					selfRegister();
				showOverlayNeighborArrow(simpleMeshmsg->getSrcNode(), false,
										 "m=m,50,0,50,0;ls=red,1");
				SimpleMeshMessage* joinAck = new SimpleMeshMessage("joinAck");
				joinAck->setCommand(JOIN_ACK);
				joinAck->setSrcNode(thisNode);
				joinAck->setBitLength(SIMPLEMESHMESSAGE_L(msg));
				sendMessageToUDP(simpleMeshmsg->getSrcNode(),joinAck);

				stat_joinACK += 1;
				stat_joinACKBytesSent += joinAck->getByteLength();
			}
			else
			{
				SimpleMeshMessage* joinDeny = new SimpleMeshMessage("joinDeny");
				joinDeny->setCommand(JOIN_DENY);
				joinDeny->setSrcNode(thisNode);
				joinDeny->setBitLength(SIMPLEMESHMESSAGE_L(msg));
				sendMessageToUDP(simpleMeshmsg->getSrcNode(),joinDeny);

				stat_joinDNY += 1;
				stat_joinDNYBytesSent += joinDeny->getByteLength();
			}
		}
		else if(simpleMeshmsg->getCommand() == JOIN_ACK)
		{
			if((int)LV->neighbors.size() <= neighborNum)
			{
				if(!isRegistered)
					selfRegister();
				stat_addedNeighbors += 1;
				showOverlayNeighborArrow(simpleMeshmsg->getSrcNode(), false,
										 "m=m,50,0,50,0;ls=red,1");
			}
			else
			{
				SimpleMeshMessage* joinDeny = new SimpleMeshMessage("joinDeny");
				joinDeny->setCommand(JOIN_DENY);
				joinDeny->setSrcNode(thisNode);
				joinDeny->setBitLength(SIMPLEMESHMESSAGE_L(msg));
				sendMessageToUDP(simpleMeshmsg->getSrcNode(),joinDeny);
				if(isInVector(simpleMeshmsg->getSrcNode(),LV->neighbors))
					deleteVector(simpleMeshmsg->getSrcNode(),LV->neighbors);
				deleteOverlayNeighborArrow(simpleMeshmsg->getSrcNode());

				stat_joinDNY += 1;
				stat_joinDNYBytesSent += joinDeny->getByteLength();
			}
		}
		else if(simpleMeshmsg->getCommand() == JOIN_DENY)
		{
			if(isInVector(simpleMeshmsg->getSrcNode(),LV->neighbors))
				deleteVector(simpleMeshmsg->getSrcNode(),LV->neighbors);
			deleteOverlayNeighborArrow(simpleMeshmsg->getSrcNode());
		}
		else if(simpleMeshmsg->getCommand() == DISCONNECT)
		{
			disconnectProcess(simpleMeshmsg->getSrcNode());
		}
		delete simpleMeshmsg;
	}
	else
		DenaCastOverlay::handleUDPMessage(msg);
}
void SimpleMesh::handleNodeGracefulLeaveNotification()
{
	neighborNum = 0;
	selfUnRegister();
	std::cout << "time: " << simTime()<< "  "<<getParentModule()->getParentModule()->getFullName() <<  std::endl;

	SimpleMeshMessage* disconnectMsg = new SimpleMeshMessage("disconnect");
	disconnectMsg->setCommand(DISCONNECT);
	disconnectMsg->setSrcNode(thisNode);
	disconnectMsg->setBitLength(SIMPLEMESHMESSAGE_L(msg));
	for (unsigned int i=0; i != LV->neighbors.size(); i++)
	{
		sendMessageToUDP(LV->neighbors[i],disconnectMsg->dup());
		deleteOverlayNeighborArrow(LV->neighbors[i]);
		stat_disconnectMessages += 1;
		stat_disconnectMessagesBytesSent += disconnectMsg->getByteLength();
	}
	VideoMessage* videoMsg = new VideoMessage();
	videoMsg->setCommand(LEAVING);
	send(videoMsg,"appOut");
	delete disconnectMsg;
}

bool SimpleMesh::isInVector(TransportAddress& Node, std::vector <TransportAddress> &neighbors)
{
	for (unsigned int i=0; i!=neighbors.size(); i++)
	{
		if (neighbors[i] == Node)
		{
			return true;
		}
	}
	return false;
}
void SimpleMesh::deleteVector(TransportAddress Node,std::vector <TransportAddress> &neighbors)
{
	for (unsigned int i=0; i!=neighbors.size(); i++)
	{

		if(neighbors[i].isUnspecified())
		{
			neighbors.erase(neighbors.begin()+i,neighbors.begin()+1+i);
			break;
		}
	}
	if(Node.isUnspecified())
		return;
	for (unsigned int i=0; i!=neighbors.size(); i++)
	{

		if (Node == neighbors[i])
		{
			neighbors.erase(neighbors.begin()+i,neighbors.begin()+1+i);
			break;
		}
	}
}

void SimpleMesh::selfRegister()
{
	DenaCastTrackerMessage* selfReg = new DenaCastTrackerMessage("selfRegister");
	selfReg->setCommand(SELF_REGISTER);
	selfReg->setSrcNode(thisNode);
	selfReg->setIsServer(isSource);
	selfReg->setRemainNeighbor(passiveNeighbors);
	sendMessageToUDP(trackerAddress,selfReg);
	isRegistered = true;
}
void SimpleMesh::selfUnRegister()
{
	if(isRegistered)
	{
		DenaCastTrackerMessage* selfUnReg = new DenaCastTrackerMessage("selfUnRegister");
		selfUnReg->setCommand(SELF_UNREGISTER);
		selfUnReg->setSrcNode(thisNode);
		selfUnReg->setIsServer(isSource);
		sendMessageToUDP(trackerAddress,selfUnReg);
		isRegistered = false;
	}
}
void SimpleMesh::disconnectProcess(TransportAddress Node)
{
	deleteVector(Node,LV->neighbors);
	deleteOverlayNeighborArrow(Node);
	if(!isSource)
	{
		cancelEvent(meshJoinRequestTimer);
		scheduleAt(simTime(),meshJoinRequestTimer);
	}
	VideoMessage* videoMsg = new VideoMessage();
	videoMsg->setCommand(NEIGHBOR_LEAVE);
	videoMsg->setSrcNode(Node);
	send(videoMsg,"appOut");
}
void SimpleMesh::finishOverlay()
{
	if(!isSource)
    	cancelAndDelete(meshJoinRequestTimer);
	else
		cancelAndDelete(serverNeighborTimer);
	globalStatistics->addStdDev("SimpleMesh: JOIN::REQ Messages", stat_joinREQ);
	globalStatistics->addStdDev("SimpleMesh: JOIN::REQ Bytes sent", stat_joinREQBytesSent);
	globalStatistics->addStdDev("SimpleMesh: JOIN::RSP Messages", stat_joinRSP);
	globalStatistics->addStdDev("SimpleMesh: JOIN::RSP Bytes sent", stat_joinRSPBytesSent);
	globalStatistics->addStdDev("SimpleMesh: JOIN::ACK Messages", stat_joinACK);
	globalStatistics->addStdDev("SimpleMesh: JOIN::ACK Bytes sent", stat_joinACKBytesSent);
	globalStatistics->addStdDev("SimpleMesh: JOIN::DNY Messages", stat_joinDNY);
	globalStatistics->addStdDev("SimpleMesh: JOIN::DNY Bytes sent", stat_joinDNYBytesSent);
	globalStatistics->addStdDev("SimpleMesh: DISCONNECT:IND Messages", stat_disconnectMessages);
	globalStatistics->addStdDev("SimpleMesh: DISCONNECT:IND Bytes sent", stat_disconnectMessagesBytesSent);
	globalStatistics->addStdDev("SimpleMesh: Neighbors added", stat_addedNeighbors);
	globalStatistics->addStdDev("SimpleMesh: Number of JOINRQ selfMessages", stat_nummeshJoinRequestTimer);
	globalStatistics->addStdDev("SimpleMesh: Download bandwidth", downBandwidth);
	globalStatistics->addStdDev("SimpleMesh: Upload bandwidth", upBandwidth);
	setOverlayReady(false);
}
