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
 * @file CDNP2PTracker.cc
 * @author Yasser Seyyedi, Behnam Ahmadifar
 */

#include "CDNP2PTracker.h"

Define_Module(CDNP2PTracker);
void CDNP2PTracker::initializeOverlay(int stage)
{
	if (stage != MIN_STAGE_OVERLAY)
		return;
    connectedMesh = par ("connectedMesh");
    serverNum = 0;
	getParentModule()->getParentModule()->setDisplayString("p=240,50;i=device/mainframe_l;i2=block/circle_vs");
	getParentModule()->getParentModule()->setName("CDNP2PTracker");
}
void CDNP2PTracker::joinOverlay()
{
	setOverlayReady(true);
}
void CDNP2PTracker::handleUDPMessage(BaseOverlayMessage* msg)
{
	if (dynamic_cast<DenaCastTrackerMessage*>(msg) != NULL)
	{
		DenaCastTrackerMessage* trackerMsg = check_and_cast<DenaCastTrackerMessage*>(msg);
		if(trackerMsg->getCommand() == NEIGHBOR_REQUEST)
		{
			DenaCastTrackerMessage* NeighborRsp = new DenaCastTrackerMessage("NeighborRsp");
			NeighborRsp->setCommand(NEIGHBOR_RESPONSE);
			NeighborRsp->setSrcNode(thisNode);
			unsigned int size = 0;
			SetServerNumber(trackerMsg->getSrcNode());
			size = calculateSize(trackerMsg->getNeighborSize(), trackerMsg->getSrcNode());
			std::vector <TransportAddress> list;
			FillList(list,trackerMsg->getSrcNode(),size);
			NeighborRsp->setNeighborsArraySize(size);
			for(unsigned int i=0 ; i<size ; i++ )
				NeighborRsp->setNeighbors(i,list[i]);
			sendMessageToUDP(trackerMsg->getSrcNode(),NeighborRsp);

		}
		else if(trackerMsg->getCommand() == SELF_REGISTER)
		{
			nodeInfo nF;
			nF.tAddress = trackerMsg->getSrcNode();
			nF.remainedNeighbor = trackerMsg->getRemainNeighbor();
			nF.timeOut = simTime().dbl();
			if(trackerMsg->getIsServer())
			{
				serverNum += 1;
				peerList.insert (std::make_pair<int,nodeInfo>(serverNum,nF));
				peerServers.insert(std::make_pair<NodeHandle,int>(trackerMsg->getSrcNode(),serverNum));
			}
			else
			{
				int selectServer = getServerNumber(trackerMsg->getSrcNode());
				peerList.insert (std::make_pair<int,nodeInfo>(selectServer,nF));
			}
		}
		else if(trackerMsg->getCommand() == SELF_UNREGISTER)
		{
			std::multimap <int,nodeInfo>::iterator nodeIt = peerList.begin();
			for(nodeIt = peerList.begin() ; nodeIt != peerList.end() ; nodeIt++)
			{
				if(nodeIt->second.tAddress.getIp() == trackerMsg->getSrcNode().getIp())
				{
					peerList.erase(nodeIt);
					break;
				}
			}
			peerServers.erase(trackerMsg->getSrcNode());
		}
		else if (trackerMsg->getCommand() == REMAIN_NEIGHBOR)
		{
			nodeInfo nF;
			nF.tAddress = trackerMsg->getSrcNode();
			nF.remainedNeighbor = trackerMsg->getRemainNeighbor();
			std::multimap <int,nodeInfo>::iterator nodeIt = peerList.begin();
			for(nodeIt = peerList.begin() ; nodeIt != peerList.end() ; nodeIt++)
				if(nodeIt->second.tAddress == nF.tAddress)
				{
					nodeIt->second.remainedNeighbor = nF.remainedNeighbor;
					nodeIt->second.timeOut =  simTime().dbl();
					break;
				}
//			checkPeersTimOuts();
		}
		delete trackerMsg;
	}
	else
		delete msg;
}
void CDNP2PTracker::finishOverlay()
{
	setOverlayReady(false);
}
void CDNP2PTracker::FillList(std::vector <TransportAddress>& list, TransportAddress& node, unsigned int size)
{
	int selectServer = getServerNumber(node);
	std::multimap <int,nodeInfo>::iterator nodeIt = peerList.begin();

	while (list.size() < size)
	{
		int randomNum = intuniform(1,peerList.size());
		for(int i=1; i<randomNum ; i++)
			++nodeIt;
		if(satisfactionConnected())
		{
			if(nodeIt != peerList.end()
					&& nodeIt->second.tAddress != node && !isInVector(nodeIt->second.tAddress,list)
					&& nodeIt->second.remainedNeighbor > 0)
				list.push_back(nodeIt->second.tAddress);
		}
		else
		{
			if(nodeIt != peerList.end() && nodeIt->first == selectServer
				&& nodeIt->second.tAddress != node && !isInVector(nodeIt->second.tAddress,list)
				&& nodeIt->second.remainedNeighbor > 0)
				list.push_back(nodeIt->second.tAddress);
		}
		nodeIt = peerList.begin();
	}
}

bool CDNP2PTracker::isInVector(TransportAddress& Node, std::vector <TransportAddress> &list)
{
	for (unsigned int i=0; i!=list.size(); i++)
	{
		if (list[i] == Node)
		{
			return true;
		}
	}
	return false;
}
int CDNP2PTracker::calculateSize(unsigned int neighborSize, TransportAddress& sourceNode)
{
	int selectServer = getServerNumber(sourceNode);
	unsigned int sectionSize = 0;
	std::multimap <int,nodeInfo>::iterator nodeIt = peerList.begin();
	if(satisfactionConnected())
	{
		for(nodeIt = peerList.begin() ; nodeIt != peerList.end() ; nodeIt++)
			if(nodeIt->second.tAddress != sourceNode && nodeIt->second.remainedNeighbor > 0)
				sectionSize += 1;
		if(sectionSize > neighborSize)
			return neighborSize;
		else
			return sectionSize;
	}
	else
	{
		for(nodeIt = peerList.begin() ; nodeIt != peerList.end() ; nodeIt++)
		{
			if(nodeIt->first == selectServer && nodeIt->second.tAddress != sourceNode && nodeIt->second.remainedNeighbor > 0)
				sectionSize += 1;
		}
		if(sectionSize > neighborSize)
			return neighborSize;
		else
			return sectionSize;
	}
}
bool CDNP2PTracker::satisfactionConnected()
{
	if(connectedMesh)
	{
		if(5*(serverNum+2) > peerList.size())
			return false;
		else
			return true;
	}
	else
		return false;

}
int CDNP2PTracker::getServerNumber(TransportAddress& node)
{
	std::map <TransportAddress,int>::iterator serverIt = peerServers.begin();
	serverIt = peerServers.find(node);
	return serverIt->second;
}
void CDNP2PTracker::SetServerNumber(TransportAddress& node)
{
	std::map <TransportAddress,int>::iterator serverIt = peerServers.begin();
	serverIt = peerServers.find(node);
	if(serverIt == peerServers.end())
	{
		int *serverPopulation = new int[serverNum+1];			// Load Balancing
		for(unsigned int i =1 ; i<serverNum+1 ; i++)
			serverPopulation[i]=0;
		for(serverIt = peerServers.begin(); serverIt != peerServers.end() ; serverIt++)
			serverPopulation[serverIt->second]++;
		int min = serverPopulation[1];
		int index = 1;
		for(unsigned int i =1 ; i<serverNum+1 ; i++)
		{
			if(min > serverPopulation[i])
			{
				min = serverPopulation[i];
				index = i;
			}
		}
		peerServers.insert(std::make_pair<TransportAddress,int>(node,index));
	}
}
void CDNP2PTracker::checkPeersTimOuts()
{
	std::multimap <int,nodeInfo>::iterator nodeIt = peerList.begin();
	for(nodeIt = peerList.begin() ; nodeIt != peerList.end() ; nodeIt++)
	{
		if(simTime() - nodeIt->second.timeOut > 10)
		{
			peerList.erase(nodeIt);
			peerServers.erase(nodeIt->second.tAddress);
			std::cout << "node: " << nodeIt->second.tAddress << "  timeOut: " << simTime() - nodeIt->second.timeOut << std::endl;
			break;
		}

	}
}
