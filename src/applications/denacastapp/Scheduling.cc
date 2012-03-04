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
 * @file Scheduling.cc
 * @author Yasser Seyyedi, Behnam Ahmadifar
 */

#include "DenaCastApp.h"
#include <GlobalStatistics.h>

void DenaCastApp::coolStreamingScheduling()
{
	resetFreeBandwidth();
	notInNeighbors.clear();

	std::multimap <int,chunkPopulation> requestingWindow;
	int nextChunk = getNextChunk();

	while(notInNeighbors.size() - requestingWindow.size() < 4 && requestingWindow.size() < 30)
	{
		chunkPopulation cP;
		cP.chunkNum = nextChunk;
		for (unsigned int i=0 ; i < neighborsBufferMaps.size();i++)
			if (neighborsBufferMaps[i].buffermap.findChunk(nextChunk))
				cP.supplierIndex.push_back(i);
		if(cP.supplierIndex.size() != 0)
			requestingWindow.insert(std::make_pair<int,chunkPopulation>(cP.supplierIndex.size(),cP));
		notInNeighbors.push_back(nextChunk);
		nextChunk = getNextChunk();
	}
	if(requestingWindow.size() < 15)
		return;
	std::multimap <int,chunkPopulation>::iterator supplierIt = requestingWindow.begin();
	while(supplierIt != requestingWindow.end())
	{
		VideoMessage *ChunkReq = new VideoMessage("Chunk_Req");
		if(supplierIt->first == 1)
		{
			ChunkReq->setCommand(CHUNK_REQ);
			Chunk CH;
			CH.setValues(chunkSize);
			CH.setChunkNumber(supplierIt->second.chunkNum);
			ChunkReq->setChunk(CH);
			ChunkReq->setDstNode(neighborsBufferMaps[supplierIt->second.supplierIndex[0]].tAddress);
			ChunkReq->setBitLength(VIDEOMESSAGE_L(msg));
			sendFrames.push_back(supplierIt->second.chunkNum);
			send(ChunkReq,"to_lowerTier");
			neighborsBufferMaps[supplierIt->second.supplierIndex[0]].freeBandwidth -= averageChunkLength;
		}
		else
		{
			int maxIndex = supplierIt->second.supplierIndex[0];
			for(int i = 1 ; i < supplierIt->first ; i++)
				if(neighborsBufferMaps[supplierIt->second.supplierIndex[i]].freeBandwidth > neighborsBufferMaps[maxIndex].freeBandwidth)
					maxIndex = supplierIt->second.supplierIndex[i];
			ChunkReq->setCommand(CHUNK_REQ);
			Chunk CH;
			CH.setValues(chunkSize);
			CH.setChunkNumber(supplierIt->second.chunkNum);
			ChunkReq->setChunk(CH);
			ChunkReq->setDstNode(neighborsBufferMaps[maxIndex].tAddress);
			ChunkReq->setBitLength(VIDEOMESSAGE_L(msg));
			sendFrames.push_back(supplierIt->second.chunkNum);
			send(ChunkReq,"to_lowerTier");
			neighborsBufferMaps[maxIndex].freeBandwidth -= averageChunkLength;
		}
		supplierIt++;
	}

}
void DenaCastApp::recieverSideScheduling2()
{
	resetFreeBandwidth();
	notInNeighbors.clear();
	int upHead = 0;
	int requestNum = 0;
	if(chunkSize >= gopSize)
	{
		upHead = 12;
		requestNum = 5;
	}
	else
	{
		upHead = 40;
		requestNum = 12;
	}
	std::multimap <int,chunkPopulation> requestingWindow;
	int nextChunk = getNextChunk();

	while(notInNeighbors.size() - requestingWindow.size() < 4)
	{
		chunkPopulation cP;
		cP.chunkNum = nextChunk;
		for (unsigned int i=0 ; i < neighborsBufferMaps.size();i++)
			if (neighborsBufferMaps[i].buffermap.findChunk(nextChunk))
				cP.supplierIndex.push_back(i);
		if(cP.supplierIndex.size() != 0)
			requestingWindow.insert(std::make_pair<int,chunkPopulation>(nextChunk,cP));
		notInNeighbors.push_back(nextChunk);
		nextChunk = getNextChunk();
	}
	if(requestingWindow.size() < upHead)
		return;
	std::multimap <int,chunkPopulation>::iterator supplierIt = requestingWindow.begin();
	int counter = 0;
	while(supplierIt != requestingWindow.end() && counter < requestNum)
	{
		counter++;
		VideoMessage *ChunkReq = new VideoMessage("Chunk_Req");
		int maxIndex = supplierIt->second.supplierIndex[0];
		for(unsigned int i = 1 ; i < supplierIt->second.supplierIndex.size() ; i++)
			if(neighborsBufferMaps[supplierIt->second.supplierIndex[i]].freeBandwidth > neighborsBufferMaps[maxIndex].freeBandwidth)
				maxIndex = supplierIt->second.supplierIndex[i];
		ChunkReq->setCommand(CHUNK_REQ);
		Chunk CH;
		CH.setValues(chunkSize);
		CH.setChunkNumber(supplierIt->second.chunkNum);
		ChunkReq->setChunk(CH);
		ChunkReq->setDstNode(neighborsBufferMaps[maxIndex].tAddress);
		ChunkReq->setBitLength(VIDEOMESSAGE_L(msg));
		sendFrames.push_back(supplierIt->second.chunkNum);
		send(ChunkReq,"to_lowerTier");
		neighborsBufferMaps[maxIndex].freeBandwidth -= averageChunkLength;
		supplierIt++;
	}

}
void DenaCastApp::recieverSideScheduling3()
{
	notInNeighbors.clear();
	int nextChunk = -1;
	while (nextChunk == -1)
	{
		nextChunk = LV->hostBufferMap->getNextUnsetChunk(sendFrames,notInNeighbors,playbackPoint);
		if(nextChunk == -1)
		{
			LV->videoBuffer->shiftChunkBuf();
			LV->updateLocalBufferMap();
		}
	}
	std::vector < int > frameSuppliers;
	while(notInNeighbors.size() < chunkSize*5)
	{
		for (unsigned int i=0 ; i < neighborsBufferMaps.size();i++)
			if (neighborsBufferMaps[i].buffermap.findChunk(nextChunk))
				frameSuppliers.push_back(i);

		if(frameSuppliers.size() != 0)
			break;
		else
		{
			notInNeighbors.push_back(nextChunk);
			nextChunk = LV->hostBufferMap->getNextUnsetChunk(sendFrames,notInNeighbors,playbackPoint);
		}
	}
	if(notInNeighbors.size() >= chunkSize*5 )    //gopSize*2 is number of forward frames
		scheduleAt(simTime()+getRequestFramePeriod(),requestChunkTimer);
	else
	{
		globalStatistics->addStdDev("DenaCastApp: Number of suppliers", frameSuppliers.size());
		int neighborIndex = intuniform(0,frameSuppliers.size()-1);
		neighborsBufferMaps[frameSuppliers[neighborIndex]].requestCounter += 1;//framesList.size();

		VideoMessage *ChunkRequest = new VideoMessage("Chunk_Req");
		ChunkRequest->setCommand(CHUNK_REQ);
		Chunk cH;
		cH.setChunkNumber(nextChunk);
		ChunkRequest->setChunk(cH);
		ChunkRequest->setDstNode(neighborsBufferMaps[frameSuppliers[neighborIndex]].tAddress);
		ChunkRequest->setBitLength(VIDEOMESSAGE_L(msg));
		ChunkRequest->setDeadLine((double)(nextChunk - playbackPoint)/(double)Fps + simTime().dbl());
		requestedChunks.push_back(nextChunk);
		sendFrames.push_back(nextChunk);
		scheduleAt(simTime()+getRequestFramePeriod(),requestChunkTimer);
		send(ChunkRequest,"to_lowerTier");
	}
}
double DenaCastApp::getRequestFramePeriod()
{
	if(playingState == BUFFERING)
		return 1/(double)Fps;
	else if(playingState == PLAYING)
		return 1/(double)Fps - 0.01;
	else
		return 1000;
}
int DenaCastApp::requestRateState()
{
	if(!rateControl)
		return 0;
	if(playingState == BUFFERING)
		return 0;
	double bufferedTime = (double)(LV->videoBuffer->lastSetFrame - playbackPoint);
	bufferedTime = bufferedTime/Fps;
	if(bufferedTime > startUpBuffering*2/5)
		return 0;
	if(bufferedTime <= startUpBuffering*2/5 &&  bufferedTime > startUpBuffering/4)
		return 1;
	else
		return 2;

}
void DenaCastApp::senderSideScheduling1()
{
	handleChunkRequest(senderqueue.begin()->second.tAddress,senderqueue.begin()->second.chunkNo,false);
	senderqueue.erase(senderqueue.begin());
}
void DenaCastApp::senderSideScheduling2()
{
	if(sendFrameTimer->isScheduled())
		return;
	else
	{
		if(senderqueue.size() != 0)
		{
			handleChunkRequest(senderqueue.begin()->second.tAddress,senderqueue.begin()->second.chunkNo,false);
			Chunk CH = LV->videoBuffer->getChunk(senderqueue.begin()->second.chunkNo);
			scheduleAt(simTime() + 8*(CH.getChunkByteLength()+29)/LV->getUpBandwidth(),sendFrameTimer);
			senderqueue.erase(senderqueue.begin());
		}
		else
			return;
	}
}
void DenaCastApp::selectRecieverSideScheduling()
{ // Receive
	switch(receiverSideSchedulingNumber)
	{
	case 1:
		coolStreamingScheduling();
		break;
	case 2:
		recieverSideScheduling2();
		break;
	case 3:
		recieverSideScheduling3();
		break;
	default:
		recieverSideScheduling2();
		break;
	}
}
void DenaCastApp::selectSenderSideScheduling()
{
	switch(senderSideSchedulingNumber)
	{
	case 1:
		senderSideScheduling1();
		break;
	case 2:
		senderSideScheduling2();
		break;
	default:
		senderSideScheduling1();
		break;
	}
}
void DenaCastApp::handleChunkRequest(TransportAddress& SrcNode,int chunkNo, bool push)
{
	VideoMessage *chunkRsp = new VideoMessage("Chunk_Rsp");
	chunkRsp->setDstNode(SrcNode);
	countRequest(SrcNode);
	if(LV->hostBufferMap->findChunk(chunkNo))
	{
		Chunk CH = LV->videoBuffer->getChunk(chunkNo);
		if(isVideoServer)
			CH.setHopCout(0);
		chunkRsp->setCommand(CHUNK_RSP);
		chunkRsp->setChunk(CH);

		chunkRsp->setByteLength(CH.getChunkByteLength()+ VIDEOMESSAGE_L(msg)/8);
		send(chunkRsp,"to_lowerTier");
	}
	else
		delete chunkRsp;
}
void DenaCastApp::resetFreeBandwidth()
{
	for(unsigned int i = 0; i< neighborsBufferMaps.size(); i++)
		neighborsBufferMaps[i].freeBandwidth = neighborsBufferMaps[i].totalBandwidth;
}
int DenaCastApp::getNextChunk()
{
	int	deadLineFrame = playbackPoint+2*gopSize;
	int nextChunk = -1;
	while (nextChunk == -1)
	{
		nextChunk = LV->hostBufferMap->getNextUnsetChunk(sendFrames,notInNeighbors,(playbackPoint+2*gopSize)/chunkSize);
		if(nextChunk == -1)
		{
			LV->videoBuffer->shiftChunkBuf();
			LV->updateLocalBufferMap();
		}
	}
	return nextChunk;
}
