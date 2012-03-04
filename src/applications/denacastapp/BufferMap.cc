//
// Copyright (C) 2010 DenaCast All Rights Reserved.
// http://www.denacast.com
// The DenaCast was designed and developed at the DML(Digital Media Lab http://dml.ir/)
// under supervision of Dr. Behzad Akbari
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
 * @file BufferMap.cc
 * @author Behnam Ahmadifar, Yasser Seyyedi
 */

#include "BufferMap.h"

#include <iostream>

void BufferMap::setLastSetChunk(int LastSetChunk)
{
	lastSetChunk = LastSetChunk;
}
void BufferMap::setValues(int BufferSize)
{
	bufferSize = BufferSize;
	lastSetChunk = 0;
	buffermap = new bool[bufferSize];
	chunkNumbers = new short int[bufferSize];
	for(int i = 0 ; i < bufferSize ; i++)
	{
		buffermap[i] = false;
		chunkNumbers[i]=i;
	}
}
bool BufferMap::findChunk(int ChunkNumber)
{
	if(ChunkNumber - chunkNumbers[0] >= bufferSize || ChunkNumber - chunkNumbers[0] < 0)
		return false;
	if(buffermap[ChunkNumber - chunkNumbers[0]])
		return true;
	else
		return false;
}
std::ostream& operator<<(std::ostream& os, const BufferMap& b)
{
	for(int i = 0 ; i < b.bufferSize ; i++)
	{
		os << b.chunkNumbers[i] << ": " << b.buffermap[i] << "  ";
		if((i+1)%12 == 0)
			os << std::endl;
	}
	return os;
}
int BufferMap::getNextUnsetChunk(std::vector<int>& sendFrames ,
				std::vector<int>& notInNeighbors, int playbackPoint)
{
	int unSetChunk = -1;
	bool c1 = true;
	bool c2 = true;
	for(int i=0 ; i<bufferSize ; i++)
	{
		unSetChunk = chunkNumbers[i];
		if(playbackPoint > unSetChunk)
			continue;
		c1=true;
		c2=true;
		if(!buffermap[i])
		{
			for (unsigned int k=0; k!=sendFrames.size(); k++)
			{
				if (sendFrames[k] == unSetChunk)
				{
					c1=false;
					break;
				}
			}
			if(c1)
				for (unsigned int k=0; k!=notInNeighbors.size(); k++)
				{
					if (notInNeighbors[k] == unSetChunk)
					{
						c2=false;
						break;
					}
				}
			if(c1 && c2)
				return unSetChunk;
		}
	}
	return -1;
}
int BufferMap::getBitLength()
{
	return bufferSize+16+16+16;
}
