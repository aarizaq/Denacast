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


// @file SimpleMesh.ned
// @author Behnam Ahmadifar, Yasser Seyyedi
//

package oversim.overlay.simplemesh;
import oversim.overlay.denacastoverlay.DenaCastOverlay;

simple SimpleMesh extends DenaCastOverlay
{
    parameters:
        @class(SimpleMesh);
        int activeNeighbors;
        int passiveNeighbors;
        double neighborNotificationPeriod @unit(s);
        double sourceBandwidth @unit(Mbps);
        double videoAverageRate @unit(Kbps);
        bool adaptiveNeighboring;
        bool serverGradualNeighboring;
}