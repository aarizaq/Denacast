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
 * @file MPEG4Camera.h
 * @author Behnam Ahmadifar, Yasser Seyyedi
 */

#ifndef MPEG4CAMERA_H_
#define MPEG4CAMERA_H_

#include "BaseCamera.h"
#include "VideoMessage_m.h"

class MPEG4Camera :public BaseCamera
{
	/**
	 * function for reading file specialize for MPEG4 tracefile
	 */
	void readFrame(std::ifstream * file);
};
#endif /* MPEG4CAMERA_H_ */
