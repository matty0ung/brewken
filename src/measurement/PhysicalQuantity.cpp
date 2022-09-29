/*======================================================================================================================
 * measurement/PhysicalQuantity.cpp is part of Brewken, and is copyright the following authors 2021-2022:
 *   • Matt Young <mfsy@yahoo.com>
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
 =====================================================================================================================*/
#include "measurement/PhysicalQuantity.h"

#include <QDebug>

#include "utils/EnumStringMapping.h"

namespace {
   EnumStringMapping const physicalQuantityToName {
      {QT_TR_NOOP("Mass")          , Measurement::PhysicalQuantity::Mass          },
      {QT_TR_NOOP("Volume")        , Measurement::PhysicalQuantity::Volume        },
      {QT_TR_NOOP("Time")          , Measurement::PhysicalQuantity::Time          },
      {QT_TR_NOOP("Temperature")   , Measurement::PhysicalQuantity::Temperature   },
      {QT_TR_NOOP("Color")         , Measurement::PhysicalQuantity::Color         },
      {QT_TR_NOOP("Density")       , Measurement::PhysicalQuantity::Density       },
      {QT_TR_NOOP("Mixed")         , Measurement::PhysicalQuantity::Mixed         },
      {QT_TR_NOOP("DiastaticPower"), Measurement::PhysicalQuantity::DiastaticPower}
   };
}

QString Measurement::getDisplayName(Measurement::PhysicalQuantity physicalQuantity) {
   // It's a coding error if we don't find a result (in which case EnumStringMapping::enumToString will log an error and
   // throw an exception).
   return physicalQuantityToName.enumToString(physicalQuantity);
}
