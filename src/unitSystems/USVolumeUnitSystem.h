/**
 * USVolumeUnitSystem.h is part of Brewken, and is copyright the following authors 2009-2015:
 *   • Mik Firestone <mikfire@gmail.com>
 *   • Philip Greggory Lee <rocketman768@gmail.com>
 *
 * Brewken is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * Brewken is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with this program.  If not, see
 * <http://www.gnu.org/licenses/>.
 */

#ifndef _USVOLUMEUNITSYSTEM_H
#define _USVOLUMEUNITSYSTEM_H

class USVolumeUnitSystem;

#include <QMap>
#include "unitSystems/UnitSystem.h"

class USVolumeUnitSystem : public UnitSystem
{
public:
   USVolumeUnitSystem();
   Unit* thicknessUnit(); /* Inherited from UnitSystem */
   QString unitType();

   QMap<Unit::unitScale, Unit*> const& scaleToUnit();
   QMap<QString, Unit*> const& qstringToUnit();

   Unit* unit();
};

#endif /*_USVOLUMEUNITSYSTEM_H*/
