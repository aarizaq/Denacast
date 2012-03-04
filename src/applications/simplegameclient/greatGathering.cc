//
// Copyright (C) 2006 Institut fuer Telematik, Universitaet Karlsruhe (TH)
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//

/**
* @file greatGathering.cc
* @author Helge Backhaus
*/

#include "greatGathering.h"

greatGathering::greatGathering(double areaDimension, double speed, NeighborMap *Neighbors, GlobalCoordinator* coordinator, CollisionList* CollisionRect)
               :MovementGenerator(areaDimension, speed, Neighbors, coordinator, CollisionRect)
{
    if(coordinator->getPeerCount() == 0) {
        target.x = uniform(areaDimension / 3, 2 * areaDimension / 3);
        target.y = uniform(areaDimension / 3, 2 * areaDimension / 3);
        coordinator->increasePositionSize();
        coordinator->setPosition(0, target);
        coordinator->increasePeerCount();
    }
    else {
        target = coordinator->getPosition(0);
    }
}

void greatGathering::move()
{
    flock();
    position += direction * speed;
    if(testBounds()) {
        position += direction * speed * 2;
        testBounds();
    }
}
