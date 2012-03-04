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
 * @file DenaCastApp.h
 * @author Behnam Ahmadifar, Yasser Seyyedi
 */

#ifndef DENACASTAPP_H_
#define DENACASTAPP_H_
#include "BufferMap.h"
#include "VideoMessage_m.h"
#include <BaseApp.h>
#include <TransportAddress.h>
#include <BaseCamera.h>
#include <VideoBuffer.h>
#include "LocalVariables.h"

struct requesterNode
{
	TransportAddress tAddress; /**< Transport address of the node that request a chunk*/
	short int chunkNo; /**< the chunk number that the node is requested*/
};
struct nodeBufferMap
{
	TransportAddress tAddress; /**< Transport address of a neighbor */
	BufferMap buffermap; /**< the BufferMap object of the node*/
	double totalBandwidth; /**< the total bandwidth capacity of a node*/
	double freeBandwidth; /**< the free bandwidth of neighbor in a defined time slot*/
	int requestCounter; /**< a number that shows number of requested - number of response chunk*/
};
struct chunkPopulation
{
	int chunkNum; /**< chunk number */
	std::vector <int>supplierIndex; /**< vector that keeps supplier index of neighborsBufferMaps vector*/
};
enum PlayingState
{
    PLAYING = 0,
    BUFFERING = 1,
    STOP = 2
};
class DenaCastApp : public BaseApp
{
public:
    /**
     * initializes base class-attributes
     *
     * @param stage the init stage
     */
    virtual void initializeApp(int stage);
    virtual void finishApp();
    virtual void handleTimerEvent(cMessage* msg);
    virtual void handleLowerMessage(cMessage* msg);
    virtual void handleUpperMessage(cMessage* msg);
	/**
	 * delete a frame number from sendframes vector (after timeout)
	 * @param framenum frame Number to delete
	 * @param sendFrames vector of frame that is already send
	 */
	virtual void deleteElement(int frameNum, std::vector<int>& sendFrames);
	/**
	 * broadcast buffermap to all the node neighbor
	 */
	virtual void bufferMapExchange();
	/**
	 * check if we whether enough frames eqaul to startup buffering to play
	 */
	virtual void checkForPlaying();




protected:
    unsigned short int numOfBFrame; /**<Number of B frames between 'I' and 'P' or between two 'P' frames */
    unsigned short int bufferSize; /**<number of chunk in buffer */
	unsigned short int chunkSize; /**< Number of frames in a chunk */
	unsigned short int gopSize; /**< number of frame available in one GoP (Group of picture)*/
    double bufferMapExchangePeriod; /**<Period in which we push our BufferMap to our neighbors */
    bool isVideoServer; /**<store parameter isVideoServer, if true it is video server */
    unsigned short int Fps; /**< Store parameter Fps, Frame per second of the video*/
    int playbackPoint; /**< store the frame number of the frame that recently send to player module*/
    PlayingState playingState; /**< when we send first message to player module it become true*/
    bool bufferMapExchangeStart; /**< as first bufferMap received we start to exchange our bufferMap too and it become ture*/
    double startUpBuffering; /** < How much video (second) should buffer in order to play*/
    bool rateControl; /**< if true use rate control mechanism*/
    double measuringTime; /**< the SimTime that we start collecting statistics*/
    unsigned short int receiverSideSchedulingNumber; /**< The number for selecting receiver side scheduling*/
    unsigned short int senderSideSchedulingNumber; /**< The number for selecting sender side scheduling*/
    double averageChunkLength; /**< average of one chunk length*/
    bool schedulingSatisfaction; /**< true if condition is OK for start scheduling*/


    std::vector <nodeBufferMap> neighborsBufferMaps;



	// self-messages
	cMessage* bufferMapTimer;/**< timer message to send buffermap to the neighbors */
	cMessage* requestChunkTimer; /**< time message to request chunk(s) from neighbors */
	cMessage* sendFrameTimer; /**< timer that is used in order to call sender side scheduling */
	cMessage* playingTimer; /**< timer in which send self message in 1/fps to send buffered frames to the player */

	BaseCamera* bc; /**< a pointer to base camera*/
	LocalVariables* LV; /** < a pointer to LocalVariables module*/

	//Scheduling
	virtual void coolStreamingScheduling();
	virtual void recieverSideScheduling2();
	virtual void recieverSideScheduling3();
	virtual double getRequestFramePeriod();
	virtual int requestRateState();
	virtual void senderSideScheduling1();
	virtual void senderSideScheduling2();
	virtual void selectRecieverSideScheduling();
	virtual void selectSenderSideScheduling();
	virtual void handleChunkRequest(TransportAddress& SrcNode,int chunkNo, bool push);
	virtual void countRequest(TransportAddress& node);
	virtual void sendFrameToPlayer();
	virtual void deleteNeighbor(TransportAddress& node);
	virtual void sendframeCleanUp();
	virtual bool isMeasuring(int frameNo);
	virtual void resetFreeBandwidth();
	virtual void updateNeighborBMList(BufferMapMessage* BufferMap_Recieved);
	bool isInVector(TransportAddress& Node, std::vector <TransportAddress> &list);
	void checkAvailability_RateControlLoss();
	int getNextChunk();

	/**<
	 *  Vector in which keeps the sending frames
	 * to prevent duplicate request.
	 */
	std::vector< int > sendFrames;
	/**<
	 * Vector in which keeps the frames that is next frame for requesting
	 * but they are not in neighbors (for exclusion). When the BufferMap message
	 * received it will be cleared
	 */
	std::vector < int > notInNeighbors;
	/**<
	 * Vector for keep bufferMap of neighbors
	 */

	std::vector <int> requestedChunks;
	std::vector <double> neighborHops;
	std::map <double,requesterNode> senderqueue;

	//statistics

	double stat_startupDelay;/**<
	 * Time (a random variable) that elapse between
	 * start to buffering (selecting a streaming channel)
	 * and start to playing
	 */
	double stat_startSendToPlayer; /**< Time to start to send frames to player module*/
	double stat_startBuffering; /**< Time that we start to buffering (select channel)*/
	double stat_startBufferMapExchange; /**< The time that node start to exchange it bufferMap*/
	double stat_TotalReceivedSize; /** < */
	double stat_RedundentSize; /** < */
};

#endif /* DENACASTAPP_H_ */
