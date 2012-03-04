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
 * @file MPEG4Camera.cc
 * @author Behnam Ahmadifar, Yasser Seyyedi
 */


#include "MPEG4Camera.h"
#include <GlobalStatistics.h>

Define_Module(MPEG4Camera);

void MPEG4Camera::readFrame(std::ifstream * file)
{
	VideoMessage* cameramessage = new VideoMessage("Camera-Msg");
	char Parameter[4][20];
	(* file) >> Parameter[0] >> Parameter[1] >> Parameter[2] >> Parameter[3] ;

	updateTime(atol(Parameter[0]));
	int frame_num = atoi(Parameter[0]);
	char frame_type=Parameter[1][0];
	int frame_length  = atoi(Parameter[3]); // the traces contains size in bytes
	//change  frame numbers in time order
	if(frame_num==1)
	{
		frame_num-=1;
		stat_totalIframesent +=1;
	}
	else if(frame_type=='I' && frame_num !=0)
	{
		frame_num+=1;
		stat_totalIframesent +=1;
	}
	else if( frame_type == 'P')
	{
		frame_num+=1;
		stat_totalPframesent += 1;
	}
	else if(frame_type=='B')
	{
		frame_num-=2;
		stat_totalBframesent += 1;
	}
	if(strcmp(getParentModule()->getParentModule()->getFullName(),"CDN-Server[1]") == 0)
		frameInfo.insert(std::make_pair<int,int>(frame_num,frame_length));
	cameramessage->setCommand(CAMERA_MSG);
	VideoFrame vf(frame_num,frame_length, frame_type,simTime().dbl());
	cameramessage->setVFrame(vf);
	cameramessage->setByteLength(VIDEOMESSAGE_L(msg)/8 + frame_length);
	lastFrameStreamed = frame_num;
	send(cameramessage,"to_lowerTier");

	stat_totalBytesend += frame_length;
	stat_totalFramesent += 1;
}

