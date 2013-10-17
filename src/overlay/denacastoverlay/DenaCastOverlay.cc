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
 * @file DenaCastOverlay.cc
 * @author Behnam Ahmadifar, Yasser Seyyedi
 */

#include "DenaCastOverlay.h"
#include <GlobalStatistics.h>
#include <math.h>
#include <GlobalNodeList.h>
#include <SimpleInfo.h>
#include <ModuleAccess.h>

Define_Module(DenaCastOverlay);

void DenaCastOverlay::initializeOverlay(int stage)
{
    if(stage != MIN_STAGE_OVERLAY)
        return;
    firstBufferMapSending = 0.0;
    bufferMapExchangeStart = false;
    packetUnit = par("packetUnit");
    FEC = par("FEC");
    ARQ = par("ARQ");
    pMax = par("pMax");
	sourceBandwidth = par("sourceBandwidth");
    if(globalNodeList->getPeerInfo(thisNode.getIp())->getTypeID() == par("denacastSourceNodeType").longValue())
    	isSource = true;
    else
    	isSource = false;
	if(isSource)
		getParentModule()->getParentModule()->setName("CDN-Server");

    LV = check_and_cast<LocalVariables*>(getParentModule()->getParentModule()->getSubmodule("tier1")->getSubmodule("localvariables"));
    setBandwidth();
    stat_TotalReceived = 0;
    stat_TotalErrorReceived = 0;
    stat_TotalByte = 0;
    stat_FECRedundent = 0;

}
DenaCastOverlay::~DenaCastOverlay()
{
	chunkBuffer.empty();
}
void DenaCastOverlay::handleAppMessage(cMessage* msg)
{
	if (dynamic_cast<BufferMapMessage*>(msg) != NULL)
	{
		BufferMapMessage* bufferMapmsg = dynamic_cast<BufferMapMessage*>(msg);
		EncapBufferMap* denaCastOvelayMsg = new EncapBufferMap ("Encapsulated-BufferMap");
		bufferMapmsg->setSrcNode(thisNode);
		denaCastOvelayMsg->setByteLength(ENCAPBUFFERMAP_L(msg)/8);
		bufferMapmsg->setTotalBandwidth(getUpBandwidth()/1024);
		denaCastOvelayMsg->encapsulate(bufferMapmsg);
		if(!bufferMapExchangeStart)
		{
			bufferMapExchangeStart = true;
			firstBufferMapSending = simTime().dbl();
		}
		for (unsigned int i=0 ; i!=LV->neighbors.size(); i++)
			sendMessageToUDP(LV->neighbors[i], denaCastOvelayMsg->dup());
		delete denaCastOvelayMsg;
	}
	else if (dynamic_cast<VideoMessage*>(msg) != NULL)
	{
		VideoMessage* videoBufferMsgApp=dynamic_cast<VideoMessage*>(msg);
		if(videoBufferMsgApp->getCommand() == CHUNK_RSP)
			packetizeAndSendToUDP(videoBufferMsgApp);
		else
		{
			EncapVideoMessage* denaCastOvelayMsg = new EncapVideoMessage ("Encapsulated-VideoMsg");
			char buf[50];
			sprintf(buf,"Encap-%s",videoBufferMsgApp->getName());
			denaCastOvelayMsg->setName(buf);
			videoBufferMsgApp->setSrcNode(thisNode);
			NodeHandle Dst = videoBufferMsgApp->getDstNode();
			denaCastOvelayMsg->setByteLength(ENCAPVIDEOMESSAGE_L(msg)/8);
			denaCastOvelayMsg->encapsulate(videoBufferMsgApp);
			sendMessageToUDP(Dst , denaCastOvelayMsg);
		}
	}
	else
		delete msg;
}
void DenaCastOverlay::handleUDPMessage(BaseOverlayMessage* msg)
{
	if (dynamic_cast<EncapBufferMap*>(msg) != NULL)
	{
		EncapBufferMap* UDPmsg=dynamic_cast<EncapBufferMap*>(msg);
		BufferMapMessage* bufferMapmsg = dynamic_cast<BufferMapMessage*>(UDPmsg->decapsulate()) ;
		send(bufferMapmsg,"appOut");
		delete UDPmsg;
	}
	else if (dynamic_cast<EncapVideoMessage*>(msg) != NULL)
	{
		EncapVideoMessage* denaCastOvelayMsg=dynamic_cast<EncapVideoMessage*>(msg);
		VideoMessage* videoMsgUDP=  check_and_cast<VideoMessage*> (msg->decapsulate());
		if(videoMsgUDP->getCommand() == CHUNK_RSP)
			if(FEC)
				bufferAndSendToApp(videoMsgUDP, denaCastOvelayMsg->hasBitError(),
						denaCastOvelayMsg->getLength(), denaCastOvelayMsg->getSeqNo(),
						denaCastOvelayMsg->getRedundant());
			else if(ARQ && msg->hasBitError())
				handleARQ(videoMsgUDP,
						denaCastOvelayMsg->getLength(), denaCastOvelayMsg->getSeqNo());
			else
				bufferAndSendToApp(videoMsgUDP, denaCastOvelayMsg->hasBitError(),
										denaCastOvelayMsg->getLength(), denaCastOvelayMsg->getSeqNo(),
										denaCastOvelayMsg->getRedundant());

		else
			send(videoMsgUDP,"appOut");
		delete denaCastOvelayMsg;
	}
	else if(dynamic_cast<ErrorRecoveryMessage*>(msg) != NULL)
	{
		ErrorRecoveryMessage* ARQMsg = dynamic_cast<ErrorRecoveryMessage*>(msg);
		retransmitPAcket(ARQMsg);
	}
	else
		delete msg;
}
void DenaCastOverlay::packetizeAndSendToUDP(VideoMessage* videoMsgApp)
{
	EncapVideoMessage* denaCastOvelayMsg = new EncapVideoMessage ("Encap-FrameResponse");
	videoMsgApp->setSrcNode(thisNode);
	NodeHandle Dst = videoMsgApp->getDstNode();
	denaCastOvelayMsg->setLength(videoMsgApp->getByteLength());

	int byteleft = videoMsgApp->getByteLength();
	int segmentNumber;
	if(byteleft%packetUnit == 0)
		segmentNumber = byteleft/packetUnit ;
	else
		segmentNumber = byteleft/packetUnit +1;
	int redundantNo = 0;
	if(FEC)
		redundantNo = getNumFECRedundantPackets(segmentNumber);
	byteleft += redundantNo*packetUnit;
	stat_FECRedundent += redundantNo*packetUnit + redundantNo*20;
	int seqNo = 1;
	int ByteLength;
	VideoMessage* videoBufferMsgTmp;
	while (byteleft != 0)
	{
		if(byteleft>packetUnit)
		{
			ByteLength = packetUnit;
			byteleft -= packetUnit;
		}
		else
		{
			ByteLength = byteleft;
			byteleft = 0;
		}
		char buf[50];
		sprintf(buf,"Encap-Packet-#%d",seqNo);
		videoMsgApp->setByteLength(ByteLength);
		denaCastOvelayMsg->setSeqNo(seqNo);
		denaCastOvelayMsg->setRedundant(redundantNo);
		denaCastOvelayMsg->setByteLength(ENCAPVIDEOMESSAGE_PACKET_L(msg)/8);
		denaCastOvelayMsg->setName(buf);
		denaCastOvelayMsg->encapsulate(videoMsgApp->dup());
		stat_TotalByte += denaCastOvelayMsg->getByteLength();
		sendMessageToUDP(Dst , denaCastOvelayMsg->dup());

		videoBufferMsgTmp=  check_and_cast<VideoMessage*> (denaCastOvelayMsg->decapsulate());
		delete videoBufferMsgTmp;
		seqNo++;
	}
	delete videoMsgApp;
	delete denaCastOvelayMsg;
}

void DenaCastOverlay::handleARQ(VideoMessage* videoMsgUDP, int length, int seq)
{
	ErrorRecoveryMessage* ARQMsg = new ErrorRecoveryMessage("retransmission");
	ARQMsg->setSeqNo(seq);
	ARQMsg->setChunkNo(videoMsgUDP->getChunk().getChunkNumber());
	ARQMsg->setLength(length);
	ARQMsg->setPacketLength(videoMsgUDP->getByteLength());
	ARQMsg->setSrcNode(thisNode);
	ARQMsg->setByteLength(ERRORRECOVERYMESSAGE_L(msg)/8);
	sendMessageToUDP(videoMsgUDP->getSrcNode(),ARQMsg);
	delete videoMsgUDP;
}
void DenaCastOverlay::retransmitPAcket(ErrorRecoveryMessage* ARQMsg)
{

	Chunk cH = LV->videoBuffer->getChunk(ARQMsg->getChunkNo());
	VideoMessage* ChunkResponse = new VideoMessage("Chunk_Rsp");
	ChunkResponse->setCommand(CHUNK_RSP);
	ChunkResponse->setSrcNode(thisNode);
	ChunkResponse->setByteLength(ARQMsg->getLength());
	ChunkResponse->setChunk(cH);

	EncapVideoMessage* denaCastOvelayMsg = new EncapVideoMessage ("retransmit");
	denaCastOvelayMsg->setSeqNo(ARQMsg->getSeqNo());
	denaCastOvelayMsg->setLength(ARQMsg->getLength());
	denaCastOvelayMsg->setRedundant(0);
	denaCastOvelayMsg->setByteLength(ENCAPVIDEOMESSAGE_L(msg));
	char buf[50];
	sprintf(buf,"Encap-Retransmit-#%d",ARQMsg->getSeqNo());
	denaCastOvelayMsg->setName(buf);
	denaCastOvelayMsg->encapsulate(ChunkResponse);
	sendMessageToUDP(ARQMsg->getSrcNode(),denaCastOvelayMsg);
	delete ARQMsg;
}
void DenaCastOverlay::bufferAndSendToApp(VideoMessage* videoMsgUDP, bool hasBitError, int length, int seqNo, int redundant)
{
	stat_TotalReceived++;
	if(hasBitError)
		stat_TotalErrorReceived++;
	unsigned int i=0;
	for(i=0 ;i<chunkBuffer.size();i++)
		if(chunkBuffer[i].getChunk().getChunkNumber() == videoMsgUDP->getChunk().getChunkNumber())
			break;
	if(i==chunkBuffer.size())
	{
		Chunk cH = videoMsgUDP->getChunk();

		ChunkPackets* cHp = new ChunkPackets(packetUnit,cH,length,redundant);
		chunkBuffer.push_back(*cHp);
		chunkBuffer[i].fragmentsState[seqNo-1] = true;
		if(hasBitError && FEC)
			chunkBuffer[i].errorState[seqNo-1] = true;
		if(chunkBuffer[i].FECSatisfaction())
		{
			videoMsgUDP->setByteLength(length);
			send(videoMsgUDP,"appOut");
		}
		else
			delete videoMsgUDP;
		if(chunkBuffer[i].isComplete())
			chunkBuffer.erase(chunkBuffer.begin()+i,chunkBuffer.begin()+1+i);

	}
	else
	{
		chunkBuffer[i].fragmentsState[seqNo-1] = true;
		if(hasBitError && FEC)
			chunkBuffer[i].errorState[seqNo-1] = true;
		if(chunkBuffer[i].FECSatisfaction())
		{
			videoMsgUDP->setByteLength(length);
			send(videoMsgUDP,"appOut");
		}
		else
			delete videoMsgUDP;
		if(chunkBuffer[i].isComplete())
			chunkBuffer.erase(chunkBuffer.begin()+i,chunkBuffer.begin()+1+i);
	}
}
int DenaCastOverlay::factorial(int x)
{
	int fact = 1;
	for(int i=2; i<=x; i++)
		fact *= i;
	return fact;
}
int DenaCastOverlay::getNumFECRedundantPackets(int fragmentNo)
{
	int redundentNo = 0;
	double PER = 0;    // Packet Error Rate
	double epsilon = 0;
	if(stat_TotalReceived != 0)
		PER = (double)stat_TotalErrorReceived/(double)stat_TotalReceived;
	if(PER == 0)
		PER = 0.07;
	do
	{
		epsilon = 0;
		redundentNo++;
		for(int k = redundentNo+1; k <= fragmentNo+redundentNo ; k++)
		{
			epsilon += (double)nCr(fragmentNo+redundentNo,k)*pow(PER,k)
			*pow(1-PER,fragmentNo+redundentNo-k)
			*(double)k /(double)(fragmentNo + redundentNo);
		}
	}
	while (epsilon > pMax);
	return redundentNo;
}
void DenaCastOverlay::finishOverlay()
{
	if(stat_TotalByte != 0)
		globalStatistics->addStdDev("DenaCastOverlay: FEC Overhead", 100*(double)stat_FECRedundent/(double)stat_TotalByte);
	setOverlayReady(false);
}

void DenaCastOverlay::setBandwidth()
{

	cModule* nodeModule = getParentModule()->getParentModule();

	cModule * mobMod = findModuleWherever("mobility",nodeModule);
	if (mobMod)
	    return;

	if(!nodeModule->hasGate("pppg$i"))  //if SimpleUderlay
	{
		upBandwidth = dynamic_cast<SimpleInfo*>(globalNodeList->getPeerInfo(thisNode))->getEntry()->getTxBandwidth();
		downBandwidth = dynamic_cast<SimpleInfo*>(globalNodeList->getPeerInfo(thisNode))->getEntry()->getRxBandwidth();
//		std::cout << "upBandwidth:  " << upBandwidth<< "  downBandwidth:  " << downBandwidth << std::endl;
	}
	else //if InetUnderlay
	{
		cGate* currentGateDownload = nodeModule->gate("pppg$i",0);
		cGate* currentGateUpload = nodeModule->gate("pppg$o",0);
		if(!isSource)
		{	if (currentGateDownload->isConnected() && currentGateUpload->isConnected())
            {
            	downBandwidth = check_and_cast<cDatarateChannel *>
                    (currentGateDownload->getPreviousGate()->getChannel())->getDatarate();
				upBandwidth = check_and_cast<cDatarateChannel *>
					(currentGateUpload->getChannel())->getDatarate();
            }
		}
		else
		{
			if (currentGateUpload-> getPreviousGate()->isConnected())
			{
				upBandwidth = sourceBandwidth*1024*1024;
				check_and_cast<cDatarateChannel *>
				(currentGateUpload->getChannel())->setDatarate(upBandwidth);
			}
			if (currentGateDownload->isConnected())
			{
				downBandwidth = sourceBandwidth*1024*1024;
				check_and_cast<cDatarateChannel *>
				(currentGateDownload->getPreviousGate()->getChannel())->setDatarate(downBandwidth);
			}
		}
	}
	LV->setUpBandwidth(upBandwidth);
	LV->setDownBandwidth(downBandwidth);
}


