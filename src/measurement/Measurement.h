/*======================================================================================================================
 * measurement/Measurement.h is part of Brewken, and is copyright the following authors 2010-2022:
 *   • Mark de Wever <koraq@xs4all.nl>
 *   • Matt Young <mfsy@yahoo.com>
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
 =====================================================================================================================*/
#ifndef MEASUREMENT_SYSTEMSOFMEASUREMENT_H
#define MEASUREMENT_SYSTEMSOFMEASUREMENT_H
#pragma once

#include <QObject>
#include <QPair>
#include <QString>

#include "measurement/Unit.h"
#include "measurement/UnitSystem.h"

class BtStringConst;
class NamedEntity;

namespace Measurement {
   void loadDisplayScales();
   void saveDisplayScales();

   /**
    * \brief Set the display \c UnitSystem for the specified \c PhysicalQuantity
    *        Obviously it is a requirement that the caller ensure that physicalQuantity == unitSystem.getPhysicalQuantity()
    */
   void setDisplayUnitSystem(PhysicalQuantity physicalQuantity, UnitSystem const & unitSystem);

   /**
    * \brief Set the supplied \c UnitSystem as the display \c UnitSystem for the \c PhysicalQuantity to which it relates
    */
   void setDisplayUnitSystem(UnitSystem const & unitSystem);

   /**
    * \brief Get the display \c UnitSystem for the specified \c PhysicalQuantity
    */
   UnitSystem const & getDisplayUnitSystem(PhysicalQuantity physicalQuantity);

   /**
    * \brief Returns the \c Unit (usually a Metric/SI one where this is an option) that we use for storing a given
    *        \c PhysicalQuantity
    */
   Unit const & getUnitForInternalStorage(PhysicalQuantity physicalQuantity);

   /*!
    * \brief Converts a measurement (aka amount) to a displayable string in the appropriate units.
    *        If "units" is null, just return the amount.
    *
    * \param amount the amount to display
    * \param units the units that \c amount is in
    * \param precision how many decimal places
    * \param displayUnitSystem which unit system to use, defaulting to \c nullptr which means use the system default
    * \param displayScale which scale to use, defaulting to \c Measurement::UnitSystem::noScale which means use the
    *                     largest scale that generates a value > 1
    */
   QString displayAmount(double amount,
                         Measurement::Unit const * units = nullptr,
                         int precision = 3,
                         Measurement::UnitSystem const * displayUnitSystem = nullptr,
                         Measurement::UnitSystem::RelativeScale displayScale = Measurement::UnitSystem::noScale);

   /*!
    * \brief Converts a measurement (aka amount) to a displayable string in the appropriate units.
    *
    * \param namedEntity Named Entity of which we want to display a property
    * \param guiObject the GUI object doing the display, used to access configured unit system & scale
    * \param propertyName the \c QObject::property of \c namedEntity that returns the amount we wish to display
    * \param units which unit system it is in
    * \param precision how many decimal places to use, defaulting to 3
    */
   QString displayAmount(NamedEntity * namedEntity,
                         QObject * guiObject,
                         BtStringConst const & propertyName,
                         Measurement::Unit const * units = nullptr,
                         int precision = 3);

   /*!
    * \brief Converts a measurement (aka amount) to a displayable string in the appropriate units.
    *
    * \param amount the amount to display
    * \param section the name of the object to reference to get unit system & scales from the config file
    * \param propertyName the property name to complete the lookup for units&scales
    * \param units which unit system it is in
    * \param precision how many decimal places to use, defaulting to 3
    */
   QString displayAmount(double amount,
                         BtStringConst const & section,
                         BtStringConst const & propertyName,
                         Measurement::Unit const * units = nullptr,
                         int precision = 3);

   /*!
    * \brief Converts a measurement (aka amount) to its numerical equivalent in the specified or default units.
    *
    * \param amount the amount to display
    * \param units the units that \c amount is in
    * \param displayUnitSystem which unit system to use, defaulting to \c nullptr which means use the system default
    * \param displayScale which scale to use, defaulting to \c Measurement::UnitSystem::noScale which means use the
    *                      largest scale that generates a value > 1
    */
   double amountDisplay(double amount,
                        Unit const * units = nullptr,
                        Measurement::UnitSystem const * displayUnitSystem = nullptr,
                        Measurement::UnitSystem::RelativeScale displayScale = Measurement::UnitSystem::noScale);

   /*!
    * \brief Converts a measurement (aka amount) to its numerical equivalent in the specified or default units.
    *
    * \param namedEntity Named Entity of which we want to display a property
    * \param guiObject the GUI object doing the display, used to access configured unit system & scale
    * \param propertyName the \c QObject::property of \c namedEntity that returns the amount we wish to display
    * \param units the units that the measurement (amount) is in
    */
   double amountDisplay(NamedEntity* namedEntity,
                        QObject* guiObject,
                        BtStringConst const & propertyName,
                        Unit const * units = nullptr);

   /**
    * \brief Converts a range (ie min/max pair) of measurements (aka amounts) to its numerical equivalent in whatever
    *        units are configured for this property.
    *
    * \param namedEntity Named Entity of which we want to display a property
    * \param guiObject the GUI object doing the display, used to access configured unit system & scale
    * \param propertyNameMin
    * \param propertyNameMax
    * \param units the units that the measurement (amount) is in
    */
   QPair<double,double> displayRange(NamedEntity* namedEntity,
                                     QObject *guiObject,
                                     BtStringConst const & propertyNameMin,
                                     BtStringConst const & propertyNameMax,
                                     Unit const * units = nullptr);

   /**
    * \brief Converts a range (ie min/max pair) of measurements (aka amounts) to its numerical equivalent in whatever
    *        units are configured for this property.
    *
    * \param guiObject the GUI object doing the display, used to access configured unit system & scale
    * \param propertyName
    * \param min
    * \param max
    * \param units the units that the measurement (amount) is in
    */
   QPair<double,double> displayRange(QObject *guiObject,
                                     BtStringConst const & propertyName,
                                     double min,
                                     double max,
                                     Unit const & units);

   //! \brief Displays thickness in appropriate units from standard thickness in L/kg.
   QString displayThickness(double thick_lkg, bool showUnits = true);
   //! \brief Appropriate thickness units will be placed in \c *volumeUnit and \c *weightUnit.
   void getThicknessUnits(Unit const ** volumeUnit, Unit const ** weightUnit);

   /*!
    * \return SI amount for the string.  Similar to \c Measurement::UnitSystem::qstringToSI
    *
    * \param qstr The string to convert - typically an amount typed in by the user
    * \param physicalQuantity Caller will already know whether the amount is a mass, volume, temperature etc, so they
    *                         should tell us via this parameter. (.:TBD:. We actually know this at compile-time, so we
    *                         could make it a template parameter, but I'm not sure it's worth the bother.)
    * \param displayUnitSystem If supplied, this is the \c Measurement::UnitSystem configured for the field the user is
    *                          entering
    * \param relativeScale If supplied, this is the \c Measurement::UnitSystem::RelativeScale configured for the field
    *                      the user is entering
    */
   double qStringToSI(QString qstr,
                      Measurement::PhysicalQuantity const physicalQuantity,
                      Measurement::UnitSystem const * displayUnitSystem = nullptr,
                      Measurement::UnitSystem::RelativeScale relativeScale = Measurement::UnitSystem::noScale);

   Measurement::UnitSystem const * getUnitSystemForField(QString field, QString section);
   Measurement::UnitSystem::RelativeScale getRelativeScaleForField(QString field, QString section);

   void setUnitSystemForField(QString field, QString section, Measurement::UnitSystem const * unitSystem);
   void setRelativeScaleForField(QString field, QString section, Measurement::UnitSystem::RelativeScale relativeScale);

   //! \return true iff the string has a valid unit substring at the end.
   bool hasUnits(QString qstr);

}

#endif
