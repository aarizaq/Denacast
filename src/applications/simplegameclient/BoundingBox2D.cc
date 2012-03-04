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
 * @file BoundingBox2D.cc
 * @author Helge Backhaus
 */

#include <BoundingBox2D.h>

BoundingBox2D::BoundingBox2D() {}

BoundingBox2D::BoundingBox2D(Vector2D tl, Vector2D br)
{
    this->tl = tl;
    this->br = br;
}

BoundingBox2D::BoundingBox2D(double tlx, double tly, double brx, double bry)
{
    tl.x = tlx;
    tl.y = tly;
    br.x = brx;
    br.y = bry;
}

BoundingBox2D::BoundingBox2D(Vector2D center, double width)
{
    tl.x = center.x - width * 0.5;
    tl.y = center.y + width * 0.5;
    br.x = center.x + width * 0.5;
    br.y = center.y - width * 0.5;
}

bool BoundingBox2D::collide(const BoundingBox2D box) const
{
    if(tl.x > box.br.x)
        return false;
    if(tl.y < box.br.y)
        return false;

    if(br.x < box.tl.x)
        return false;
    if(br.y > box.tl.y)
        return false;

    return true;
}

bool BoundingBox2D::collide(const Vector2D p) const
{
    if(p.x > tl.x && p.x < br.x && p.y < tl.y && p.y > br.y)
        return true;
    else
        return false;
}

double BoundingBox2D::top()
{
    return tl.y;
}

double BoundingBox2D::bottom()
{
    return br.y;
}

double BoundingBox2D::left()
{
    return tl.x;
}

double BoundingBox2D::right()
{
    return br.x;
}

std::ostream& operator<<(std::ostream& Stream, const BoundingBox2D& box)
{
    return Stream << box.tl << " - " << box.br;
}
