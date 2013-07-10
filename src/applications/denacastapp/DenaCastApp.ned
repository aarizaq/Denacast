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


// @file DenaCastApp.ned
// @author Behnam Ahmadifar, Yasser Seyyedi
//

package oversim.applications.denacastapp;
import oversim.common.BaseApp;

//
// Application layer for DenaCast protocol
//
// @author Yasser Seyyedi, Behnam Ahmadifar 
//

simple DenaCastApp extends BaseApp
{
    parameters:
        @class(DenaCastApp);
        int gopSize;		 //Number of frames per Group of Picture
        int numOfBFrame;	//Number of B frames between 'I' and 'P' or between two 'P' frames;
        int windowOfIntrest @unit(s);  //equal to peer-side buffer
        double bufferMapExchangePeriod @unit(s);  //it means how often send bufferMap to our neighbors
        int Fps;					// Frame per second of the video
        double startUpBuffering @unit(s);	// How much time of video should buffer until playing
        bool rateControl;		// to enable and disable rateControl
        double measuringTime @unit(s);
        int receiverSideSchedulingNumber;
        int senderSideSchedulingNumber;
        int chunkSize @unit(Frames);
        double averageChunkLength @unit(Kb);
        int denacastSourceNodeType;
        @display("i=block/transport");	
}


