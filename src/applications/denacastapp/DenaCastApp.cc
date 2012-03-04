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
 * @file DenaCastApp.cc
 * @author Behnam Ahmadifar, Yasser Seyyedi
 */


#include "DenaCastApp.h"
#include <GlobalStatistics.h>
#include <GlobalNodeList.h>
#include <PeerInfo.h>
#include <LocalVariables.h>

Define_Module(DenaCastApp);


void DenaCastApp::initializeApp(int stage)
{
//	cConfiguration *cfg = getConfig();
//	cSimulation::getActiveSimulation()->getEnvir()->getConfigEx()->getAsDouble();
//	std::cout << toDouble(ev.getConfig()->getConfigValue("sim-time-limit")) << std::endl;
//	std::cout << atof(ev.getConfig()->getConfigValue("sim-time-limit")) << std::endl;
	if (stage != MIN_STAGE_APP)
		return;
	//initialize parameters
	numOfBFrame = par("numOfBFrame");
	bufferMapExchangePeriod = par("bufferMapExchangePeriod");
	gopSize = par("gopSize");
    if(globalNodeList->getPeerInfo(thisNode.getIp())->getTypeID() == 2)
    	isVideoServer = true;
    else
    	isVideoServer = false;
    startUpBuffering = par ("startUpBuffering");
    Fps = par("Fps");
    double windowOfIntrest = par("windowOfIntrest");
    chunkSize = par ("chunkSize");
    bufferSize = windowOfIntrest*Fps/chunkSize;
    if(bufferSize%chunkSize > 0)
    		bufferSize += chunkSize - (bufferSize%chunkSize);
	numOfBFrame = par("numOfBFrame");
    rateControl = par("rateControl");
    measuringTime = par("measuringTime");
    receiverSideSchedulingNumber = par("receiverSideSchedulingNumber");
    senderSideSchedulingNumber = par("senderSideSchedulingNumber");
    averageChunkLength = par("averageChunkLength");

    //initialize global variables
    playbackPoint = -1;
    playingState = BUFFERING;
    bufferMapExchangeStart = false;
    schedulingSatisfaction = false;

	//initialize self messages
	bufferMapTimer = new cMessage("bufferMapTimer");
	requestChunkTimer = new cMessage("requestChunkTimer");
	playingTimer = new cMessage("playingTimer");
	sendFrameTimer = new cMessage("sendFrameTimer");
	if(!isVideoServer)
		bc = check_and_cast<BaseCamera*>(simulation.getModuleByPath("CDN-Server[1].tier2.mpeg4camera"));

	LV = check_and_cast<LocalVariables*>(getParentModule()->getSubmodule("localvariables"));

	//statistics
	stat_startupDelay = 0;
	stat_startSendToPlayer = 0;
	stat_startBuffering = 0;
	stat_startBufferMapExchange = 0;
	stat_TotalReceivedSize = 0;
	stat_RedundentSize = 0;

	// setting server and clients display string
	if(isVideoServer)
		getParentModule()->getParentModule()->setDisplayString("i=device/server;i2=block/circle_s");
	else
		getParentModule()->getParentModule()->setDisplayString("i=device/wifilaptop_vs;i2=block/circle_s");
}
void DenaCastApp::handleTimerEvent(cMessage* msg)
{
	if(msg == sendFrameTimer)
	{
		selectSenderSideScheduling();
	}
	else if(msg == requestChunkTimer)
	{
		if(playingState != STOP)
			schedulingSatisfaction = true;
	}
	else if(msg == bufferMapTimer)
	{
		bufferMapExchange();
	}
	else if(msg == playingTimer)
	{
		if(playingState == PLAYING)
		{
			sendFrameToPlayer();
		}
		else if (playingState == BUFFERING)
		{	// check if we have video equal to startup buffering
			checkForPlaying();
		}
		scheduleAt(simTime()+1/(double)Fps,playingTimer);

	}
	else
		delete msg;

}
void DenaCastApp::handleLowerMessage(cMessage* msg)
{
	if (dynamic_cast<VideoMessage*>(msg) != NULL)
	{
		VideoMessage* VideoMsg=dynamic_cast<VideoMessage*>(msg);
		if(VideoMsg->getCommand() == CHUNK_REQ)
		{
			requesterNode rN;
			rN.tAddress = VideoMsg->getSrcNode();
			rN.chunkNo = VideoMsg->getChunk().getChunkNumber();
			senderqueue.insert(std::make_pair<double,requesterNode>(VideoMsg->getDeadLine(),rN));
			selectSenderSideScheduling();
			delete msg;
		}
		else if(VideoMsg->getCommand() == CHUNK_RSP && !isVideoServer)
		{
//			if(!bufferMapExchangeStart)
//			{
//				scheduleAt(simTime(),requestChunkTimer);
//				scheduleAt(simTime(),playingTimer);
//				scheduleAt(simTime()+bufferMapExchangePeriod+uniform(0,0.25),bufferMapTimer);
//				stat_startBufferMapExchange = simTime().dbl() + bufferMapExchangePeriod;
//				stat_startBuffering = simTime().dbl();
//				bufferMapExchangeStart=true;
//				int shiftnum = 0;
//				if(VideoMsg->getChunk().getChunkNumber() > 0)
//					shiftnum = VideoMsg->getChunk().getChunkNumber();
//				shiftnum++;
//				for(int i=0 ; i<shiftnum ; i++)
//					videoBuffer->shiftChunkBuf();
//
//				//needed for push
//			}
			bool redundantState = false;
			if(VideoMsg->getChunk().isComplete() && !VideoMsg->hasBitError())
			{
				if(LV->videoBuffer->getChunk(VideoMsg->getChunk().getChunkNumber()).isComplete())
				{
					stat_RedundentSize += VideoMsg->getChunk().getChunkByteLength();
					redundantState = true;
				}
				stat_TotalReceivedSize += VideoMsg->getChunk().getChunkByteLength();
				Chunk InputChunk = VideoMsg->getChunk();
				InputChunk.setHopCout(InputChunk.getHopCount()+1);
				LV->videoBuffer->setChunk(InputChunk);
				LV->updateLocalBufferMap();
				if(isMeasuring(VideoMsg->getChunk().getLastFrameNo()))
				{
					globalStatistics->addStdDev("DenaCastApp: end to end delay", simTime().dbl() - VideoMsg->getChunk().getCreationTime());
					globalStatistics->addStdDev("DenaCastApp: Hop Count", VideoMsg->getChunk().getHopCount());
					if(VideoMsg->getChunk().getLastFrameNo() < playbackPoint)
						LV->addToLateArivalLoss(VideoMsg->getChunk().getLateArrivalLossSize(playbackPoint));
				}
				deleteElement(VideoMsg->getChunk().getChunkNumber(),sendFrames);
			}
			delete msg;
		}
		else if(VideoMsg->getCommand() == NEIGHBOR_LEAVE)
			deleteNeighbor(VideoMsg->getSrcNode());
		else if(VideoMsg->getCommand() == LEAVING)
		{
			playingState = STOP;
			schedulingSatisfaction = false;
		}
		else
			delete VideoMsg;
	}
	else if(dynamic_cast <BufferMapMessage*> (msg) != NULL)
	{
		BufferMapMessage* BufferMap_Recieved=dynamic_cast<BufferMapMessage*>(msg);
		if(!bufferMapExchangeStart && !isVideoServer)
		{
				scheduleAt(simTime(),requestChunkTimer);
				scheduleAt(simTime(),playingTimer);
				scheduleAt(simTime()+bufferMapExchangePeriod,bufferMapTimer);
				stat_startBufferMapExchange = simTime().dbl() + bufferMapExchangePeriod;
				stat_startBuffering = simTime().dbl();
				bufferMapExchangeStart = true;
				int shiftnum = 0;
				if(BufferMap_Recieved->getBuffermap().getLastSetChunk() > 0)
					shiftnum = BufferMap_Recieved->getBuffermap().getLastSetChunk();
				if(shiftnum%gopSize > 0 && chunkSize%gopSize !=0)
					shiftnum += gopSize - (shiftnum%gopSize);
				else
					shiftnum -= 2;
				for(int i=0 ; i<shiftnum ; i++)
					LV->videoBuffer->shiftChunkBuf();
				LV->updateLocalBufferMap();
		}
		notInNeighbors.clear();
		updateNeighborBMList(BufferMap_Recieved);

		if(schedulingSatisfaction)
			selectRecieverSideScheduling();
		delete msg;
	}
	else
		delete msg;
}
void DenaCastApp::handleUpperMessage(cMessage* msg)
{
	if (dynamic_cast<VideoMessage*>(msg) != NULL)
	{
		VideoMessage* VideoMsg=dynamic_cast<VideoMessage*>(msg);
		if (VideoMsg->getCommand() == CAMERA_MSG)
		{
			if(!bufferMapExchangeStart)
			{
				scheduleAt(simTime()+bufferMapExchangePeriod,bufferMapTimer);
				bufferMapExchangeStart=true;
			}
			LV->videoBuffer->setFrame(VideoMsg->getVFrame());
			LV->updateLocalBufferMap();
			delete msg;
		}
	}
	else
		delete msg;
}
void DenaCastApp::updateNeighborBMList(BufferMapMessage* BufferMap_Recieved)
{
	bool find = false;
	for(unsigned int i=0 ; i<neighborsBufferMaps.size();i++)
		if(neighborsBufferMaps[i].tAddress == BufferMap_Recieved->getSrcNode())
		{
			neighborsBufferMaps[i].buffermap = BufferMap_Recieved->getBuffermap();
			neighborsBufferMaps[i].totalBandwidth = BufferMap_Recieved->getTotalBandwidth();
			find = true;
			break;
		}
	if(!find)
	{
		nodeBufferMap neighbourBM;
		neighbourBM.buffermap.setValues(chunkSize);
		neighbourBM.tAddress = BufferMap_Recieved->getSrcNode();
		neighbourBM.totalBandwidth =  BufferMap_Recieved->getTotalBandwidth();
		neighbourBM.requestCounter = 0;
		neighbourBM.buffermap = BufferMap_Recieved->getBuffermap();
		neighborsBufferMaps.push_back(neighbourBM);
	}
}
void DenaCastApp::deleteElement(int frameNum, std::vector<int>& sendframes)
{
	for (unsigned int i=0; i!=sendframes.size(); i++)
	{
		if (sendframes[i] == frameNum)
		{
			sendframes.erase(sendframes.begin()+i,sendframes.begin()+1+i);
			break;
		}
	}
}


void DenaCastApp::checkForPlaying()
{
	int bitCounter = 0;
	for(int i=0 ; i + LV->hostBufferMap->chunkNumbers[0] < LV->hostBufferMap->getLastSetChunk() + 1 ; i++)
		if(LV->hostBufferMap->buffermap[i])
				bitCounter++;
	bitCounter *= chunkSize;
	if(bitCounter > Fps*startUpBuffering)
	{
		playingState = PLAYING;
		stat_startSendToPlayer = simTime().dbl();
		stat_startupDelay = simTime().dbl() - stat_startBuffering;
		playbackPoint = LV->hostBufferMap->chunkNumbers[0]*chunkSize;
	}
}
void DenaCastApp::sendFrameToPlayer()
{
	VideoFrame vf = LV->videoBuffer->getFrame(playbackPoint);
	vf.setFrameNumber(playbackPoint);
	VideoMessage* playermessage = new VideoMessage("PlayerMessage");
	playermessage->setCommand(PLAYER_MSG);
	playermessage->setVFrame(vf);
	send(playermessage,"to_upperTier");
	if(vf.getFrameType() == 'N')
		checkAvailability_RateControlLoss();
	playbackPoint++;
	sendframeCleanUp();

}

void DenaCastApp::sendframeCleanUp()
{
	bool hasExpiredFrame = true;
	while (hasExpiredFrame)
	{
		hasExpiredFrame = false;
		for (unsigned int i=0; i!=sendFrames.size(); i++)
			if (sendFrames[i] < LV->videoBuffer->chunkBuffer[0].chunk[0].getFrameNumber())
				hasExpiredFrame = true;
		for (unsigned int i=0; i!=sendFrames.size(); i++)
		{
			if(sendFrames.size() < i)
				break;
			if(sendFrames[i] < LV->videoBuffer->chunkBuffer[0].chunk[0].getFrameNumber())
				sendFrames.erase(sendFrames.begin()+i,sendFrames.begin()+1+i);
		}
	}
}
bool DenaCastApp::isInVector(TransportAddress& Node, std::vector <TransportAddress> &list)
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
void DenaCastApp::checkAvailability_RateControlLoss()
{
	if(playbackPoint/Fps > measuringTime)
		return;
	bool find = false;
	for(unsigned int i = 0; i < requestedChunks.size(); i++)
	{
		if(requestedChunks[i] == playbackPoint)
		{
			requestedChunks.erase(requestedChunks.begin()+i,requestedChunks.begin()+1+i);
			find = true;
			break;
		}
	}
	if(!find)
	{
		std::map <int,int>::iterator frameIt = bc->frameInfo.begin();
		frameIt = bc->frameInfo.find(playbackPoint);
		LV->addToAvailability_RateControlLoss(frameIt->second);
	}
}

void DenaCastApp::bufferMapExchange()
{
	if(playingState != STOP)
	{
		if(LV->videoBuffer->lastSetChunk == 0)
			scheduleAt(simTime()+bufferMapExchangePeriod,bufferMapTimer);
		else
		{
			BufferMap BM;
			BM.setValues(bufferSize);
			LV->videoBuffer->updateBufferMap(&BM);
			BufferMapMessage *BufferMapmsg = new BufferMapMessage("BufferMap_exchange");
			BufferMapmsg->setBuffermap(BM);
			BufferMapmsg->setBitLength(BUFFERMAPMESSAGE_L(msg)+LV->hostBufferMap->getBitLength());
			send (BufferMapmsg,"to_lowerTier");
			scheduleAt(simTime()+bufferMapExchangePeriod,bufferMapTimer);
		}
	}
}
bool DenaCastApp::isMeasuring(int frameNo)
{
	if(frameNo/Fps > measuringTime)
		return true;
	else
		return false;
}
void DenaCastApp::countRequest(TransportAddress& node)
{
	for(unsigned int i = 0; i < neighborsBufferMaps.size(); i++)
	{
		if(neighborsBufferMaps[i].tAddress == node)
		{
			neighborsBufferMaps[i].requestCounter -= 1;
		}
	}
}
void DenaCastApp::deleteNeighbor(TransportAddress& node)
{
	for(unsigned int i=0 ; i<neighborsBufferMaps.size() ; i++)
		if(neighborsBufferMaps[i].tAddress == node)
		{
			neighborsBufferMaps.erase(neighborsBufferMaps.begin()+i,neighborsBufferMaps.begin()+1+i);
			break;
		}
}
void DenaCastApp::finishApp()
{
	cancelAndDelete(bufferMapTimer);
	cancelAndDelete(requestChunkTimer);
	cancelAndDelete(playingTimer);
	cancelAndDelete(sendFrameTimer);
	//statistics
	if(stat_startupDelay != 0)
	{
		globalStatistics->addStdDev("DenaCastApp: Startup Delay", stat_startupDelay);
		globalStatistics->recordOutVector("DenaCastApp: Startup Delay_vec", stat_startupDelay);
	}
	if(stat_startSendToPlayer != 0)
		globalStatistics->addStdDev("DenaCastApp: start to send to player time", stat_startSendToPlayer);
	if(stat_startBuffering != 0)
		globalStatistics->addStdDev("DenaCastApp: start to buffering time", stat_startBuffering);
	if(stat_startBufferMapExchange != 0)
		globalStatistics->addStdDev("DenaCastApp: start exchanging bufferMap", stat_startBufferMapExchange);
	if(stat_TotalReceivedSize != 0)
		globalStatistics->addStdDev("DenaCastApp: Frame Redundancy",stat_RedundentSize/stat_TotalReceivedSize *100);
}
