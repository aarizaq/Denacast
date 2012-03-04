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

//
//@author Behnam Ahmadifar, Yasser Seyyedi
//
cplusplus
{{
#include <BufferMap.h>
#include <TransportAddress.h>
#include "VideoFrame.h"
#include "Chunk.h"
   
// constants for message length in bit


static const int CHUNKNUMBER_L = 16;
static const int FRAMENUMBER_L = 16;
static const int FRAMETYPE_L = 2;
static const int FRAMELENGTH_L = 16;
static const int TOTALBANDWIDTH_L = 32;
static const int VIDEOCOMMAND_L = 3;

#define VIDEOMESSAGE_L(msg) (VIDEOCOMMAND_L + CHUNKNUMBER_L + 2*TRANSPORTADDRESS_L)
#define BUFFERMAPMESSAGE_L(msg) (TRANSPORTADDRESS_L + TOTALBANDWIDTH_L)
}}

class noncobject BufferMap;
class noncobject TransportAddress;
class noncobject VideoFrame;
class noncobject Chunk;

//
// Some constants for Command-Field
// @author Behnam Ahmadifar, Yasser Seyyedi
//
enum VideoCommand
{
    CAMERA_MSG=0;	// Message from Camera to VideoSource
    PLAYER_MSG=1;	// Message from VideoSource to Player
    CHUNK_REQ=2;	// Message for Requesting a frame
    CHUNK_RSP=3;	// Message for Responsing a frame
    NEIGHBOR_LEAVE=4;
    LEAVING=5;
}
//
// Main VideoBuffer-Packet definition
// @author Behnam Ahmadifar, Yasser Seyyedi
//
packet VideoMessage
{
    int command enum(VideoCommand);
    TransportAddress dstNode;	// detination node
	TransportAddress srcNode;	// source node
	double deadLine;			// deadline of chunk
	Chunk chunk;			// Chunk object
	VideoFrame VFrame;		// VideoFrame object
}
//
// BufferMap exchange packet for exchange to neighbors (pull-based)
// @author Yasser Seyyedi, Behnam Ahmadifar
//
packet BufferMapMessage
{
	TransportAddress srcNode;		// Source Node handle
    double totalBandwidth;   // Free Bandwidth
    double myHopCount;		// hop count
    BufferMap buffermap;	// BufferMap Object
}
    