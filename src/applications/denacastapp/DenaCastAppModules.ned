//
// Copyright (C) 2010 DenaCast All Rights Reserved.
// http://www.denacast.com
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

package oversim.applications.denacastapp;

import oversim.common.ITier;

//
//
// @author Yasser Seyyedi, Behnam Ahmadifar 
//

module DenaCastAppModules like ITier
{
    gates:
        input udpIn;     // gate from the UDP layer
        input from_lowerTier;    // gate from the lower tier
        input from_upperTier;    // gate from the upper tier
        output udpOut;    // gate to the UDP layer
        output to_lowerTier;    // gate to the lower tier
        output to_upperTier;    // gate to the upper tier
        input tcpIn;
        output tcpOut;
    submodules:
        denacastapp: DenaCastApp;
        localvariables: LocalVariables;
    connections allowunconnected:
        from_lowerTier --> denacastapp.from_lowerTier;
        to_lowerTier <-- denacastapp.to_lowerTier;
        to_upperTier <-- denacastapp.to_upperTier;
        from_upperTier --> denacastapp.from_upperTier;
}
