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
 * @file BaseCamera.h
 * @author Yasser Seyyedi, Behnam Ahmadifar
 */
#ifndef BASECAMERA_H_
#define BASECAMERA_H_


#include <omnetpp.h>

#include <iostream>
#include <string>
#include <fstream>

#include <BaseApp.h>

class BaseCamera : public BaseApp
{
public:
    // application routines
	virtual void initializeApp(int stage);
    virtual void handleTimerEvent(cMessage* msg);
    virtual void finishApp();
	/**
	 * base class-destructor
	 */
	~BaseCamera();
    /**
     * Read first line that contains frame information
     * @param file handle that opened for reading
     */
	virtual void readFrame(std::ifstream * file);
	/**
	 * update time substring on top of module
	 *
	 * @param frameNo current streaming frame number
	 */
	virtual void updateTime(int long frameNo);
	virtual int getLastFrameStreamed(){return lastFrameStreamed;}
	std::map <int,int> frameInfo;	// frame number, frame size

protected:

	//self messages
	cMessage* sendMessageInterval; /**< self message for sending frame to lower layer */
	cMessage* TraceFileInit; /**< self message for start streaming*/

	int lastFrameStreamed; /**< last frame that streamed*/
	std::string traceFile; /**< address of trace file in the computer*/
	std::ifstream baseFile; /**< stream reader for reading file*/
	int Fps; /**< Frame per Second of video*/
	double sendPeriod; /**< period in which we read and send frame*/
	long int totalBit;

	//statistics
	uint32_t stat_totalBytesend; /**< number of bits that received */
	uint32_t stat_totalFramesent; /**< number of frames that sent */
	uint32_t stat_totalIframesent; /**< number of I frames that sent */
	uint32_t stat_totalBframesent; /**< number of B frames that sent */
	uint32_t stat_totalPframesent; /**< number of P frames that sent */
	double stat_startStreamingTime; /**<start streaming time of this node */
};

#endif /* BASECAMERA_H_ */
