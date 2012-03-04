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
 * @file VideoFrame.cc
 * @author Behnam Ahmadifar, Yasser Seyyedi
 */

#include "VideoFrame.h"

VideoFrame::VideoFrame()
{
	setFrame(-1,-1, 'N',-1);
}
VideoFrame::VideoFrame(int FrameNumber, int FrameLength, char FrameType,double CreationTime)
{
	setFrame(FrameNumber, FrameLength,FrameType,CreationTime);
}
bool VideoFrame::isSet()
{
	if(frameType == 'N' || frameLength == -1)
		return false;
	else
		return true;
}
void VideoFrame::setFrame(VideoFrame vFrame)
{
	frameNumber = vFrame.getFrameNumber();
	frameLength = vFrame.getFrameLength();
	frameType = vFrame.getFrameType();
	creationTime = vFrame.getCreationTime();
}

VideoFrame VideoFrame::getVFrame()
{
	VideoFrame vFrame;
	vFrame.setFrame(frameNumber,frameLength,frameType,creationTime);
	return vFrame;
}
void VideoFrame::setFrame(int FrameNumber, int FrameLength, char FrameType,double CreationTime)
{
	frameNumber = FrameNumber;
	frameLength = FrameLength;
	frameType = FrameType;
	creationTime = CreationTime;
}
void VideoFrame::setFrameNumber(int FrameNumber)
{
	frameNumber = FrameNumber;
}
std::ostream& operator<<(std::ostream& os, const VideoFrame& v)
{
	os << v.frameNumber<<","
	<< v.frameType << ","
	<<v.frameLength;
	return os;
}

