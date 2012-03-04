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
 * @file BaseCamera.cc
 * @author Yasser Seyyedi, Behnam Ahmadifar
 */

#include "BaseCamera.h"
#include <GlobalStatistics.h>

Define_Module(BaseCamera);


void BaseCamera::initializeApp(int stage)
{
    if (stage != MIN_STAGE_APP) return;

	traceFile = par ("traceFile").stringValue();
	Fps=par("Fps");
	sendPeriod = 1/(double)Fps;
	sendMessageInterval = new cMessage("sendMessageInterval");
	TraceFileInit = new cMessage("TraceFileInit");
	scheduleAt(simTime(),TraceFileInit);

	lastFrameStreamed = 0;
	totalBit = 0;
	//statistics
	stat_totalBytesend = 0;
	stat_totalFramesent = 0;
	stat_totalIframesent = 0;
	stat_totalPframesent = 0;
	stat_totalBframesent =0;
	stat_startStreamingTime = 0.0;

}
BaseCamera::~BaseCamera()
{
	baseFile.close();
}
void BaseCamera::handleTimerEvent(cMessage* msg)
{

	if (msg == TraceFileInit)
	{

		baseFile.open(traceFile.c_str(), std::ios::in);
		if (baseFile.bad() || !baseFile.is_open())
		{
			opp_error("Error while opening Trace file\n");
		}
		else
		{
			stat_startStreamingTime = simTime().dbl();
			scheduleAt(simTime(),sendMessageInterval);
		}
		delete msg;

	}
	else if(msg == sendMessageInterval)
		{
			readFrame(&baseFile);
			if(!baseFile.eof())
				scheduleAt(simTime()+sendPeriod,sendMessageInterval);
		}
	else
		delete msg;
}
void BaseCamera::readFrame(std::ifstream *file)
{
}
void BaseCamera::updateTime(int long frameNo)
{
	int hour=0,min=0,sec=0;
	if (frameNo > Fps-1)
	{
		sec=frameNo/Fps;
		frameNo= frameNo%Fps;
	}
	if(sec >59)
	{
		min=sec/60;
		sec=sec%60;
	}
	if(min >59)
	{
		hour=min/60;
		min=min%60;
	}
	std::stringstream buf;
	buf << "elapsed Time:" << hour << ":" << min << ":" << sec << ":" << frameNo ;
	std::string s = buf.str();
	getParentModule()->getDisplayString().setTagArg("t", 0, s.c_str());
	getDisplayString().setTagArg("t", 0, s.c_str());

}
void BaseCamera::finishApp()
{
	cancelAndDelete(sendMessageInterval);

	// statistics
    globalStatistics->addStdDev("BaseCamera: Total send bytes", stat_totalBytesend);
    globalStatistics->addStdDev("BaseCamera: Total send frames", stat_totalFramesent);
    globalStatistics->addStdDev("BaseCamera: Start Streaming Time",stat_startStreamingTime);
    globalStatistics->addStdDev("BaseCamera: Total I frames sent", stat_totalIframesent);
    globalStatistics->addStdDev("BaseCamera: Total P frames sent", stat_totalPframesent);
    globalStatistics->addStdDev("BaseCamera: Total B frames sent", stat_totalBframesent);
}

