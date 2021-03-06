/**
 * BtLabel.cpp is part of Brewken, and is copyright the following authors 2009-2014:
 *   • Brian Rower <brian.rower@gmail.com>
 *   • Mark de Wever <koraq@xs4all.nl>
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

#include "BtLabel.h"
#include "Brewken.h"
#include <QSettings>
#include <QDebug>
#include "model/Style.h"
#include "model/Recipe.h"

/*! \brief Initialize the BtLabel with the parent and do some things with the type
 * \param parent - QWidget* to the parent object
 * \param lType - the type of label: none, gravity, mass or volume
 * \return the initialized widget
 * \todo Not sure if I can get the name of the widget being created.
 *       Not sure how to signal the parent to redisplay
 */

BtLabel::BtLabel(QWidget *parent, LabelType lType) :
   QLabel(parent)
{
   whatAmI = lType;
   btParent = parent;
   _menu = 0;

   connect(this, &QWidget::customContextMenuRequested, this, &BtLabel::popContextMenu);

}

void BtLabel::initializeSection()
{
   QWidget* mybuddy;

   if ( ! _section.isEmpty() )
      return;

   // as much as I dislike it, dynamic properties can't be referenced on
   // initialization.
   mybuddy = buddy();
   // If the label has the configSection defined, use it
   // otherwise, if the paired field has a configSection, use it
   // otherwise, if the parent object has a configSection, use it
   // if all else fails, get the parent's object name
   if ( property("configSection").isValid() )
      _section = property("configSection").toString();
   else if ( mybuddy && mybuddy->property("configSection").isValid() )
      _section = mybuddy->property("configSection").toString();
   else if ( btParent->property("configSection").isValid() )
      _section = btParent->property("configSection").toString();
   else
   {
      qDebug() << "this failed" << this;
      _section = btParent->objectName();
   }
}

void BtLabel::initializeProperty()
{
   QWidget* mybuddy;

   if ( ! propertyName.isEmpty() )
      return;

   mybuddy = buddy();
   if ( property("editField").isValid() )
      propertyName = property("editField").toString();
   else if ( mybuddy && mybuddy->property("editField").isValid() )
      propertyName = mybuddy->property("editField").toString();
   else
      qDebug() << "That failed miserably";
}

void BtLabel::initializeMenu()
{
   Unit::unitDisplay unit;
   Unit::unitScale scale;

   if ( _menu )
      return;

   unit  = (Unit::unitDisplay)Brewken::option(propertyName, Unit::noUnit, _section, Brewken::UNIT).toInt();
   scale = (Unit::unitScale)Brewken::option(propertyName, Unit::noScale, _section, Brewken::SCALE).toInt();

   switch( whatAmI )
   {
      case COLOR:
         _menu = Brewken::setupColorMenu(btParent,unit);
         break;
      case DENSITY:
         _menu = Brewken::setupDensityMenu(btParent,unit);
         break;
      case MASS:
         _menu = Brewken::setupMassMenu(btParent,unit,scale);
         break;
      case MIXED:
         // This looks weird, but it works.
         _menu = Brewken::setupVolumeMenu(btParent,unit,scale,false); // no scale menu
         break;
      case TEMPERATURE:
         _menu = Brewken::setupTemperatureMenu(btParent,unit);
         break;
      case VOLUME:
         _menu = Brewken::setupVolumeMenu(btParent,unit,scale);
         break;
      case TIME:
         _menu = Brewken::setupTimeMenu(btParent,scale); //scale menu only
         break;
      case DATE:
         _menu = Brewken::setupDateMenu(btParent,unit); // unit only
         break;
      case DIASTATIC_POWER:
         _menu = Brewken::setupDiastaticPowerMenu(btParent,unit);
         break;
      default:
         return;
   }
}

void BtLabel::popContextMenu(const QPoint& point)
{
   QObject* calledBy = sender();
   QWidget* widgie;
   QAction *invoked;

   if ( calledBy == 0 )
      return;

   widgie = qobject_cast<QWidget*>(calledBy);
   if ( widgie == 0 )
      return;

   initializeProperty();
   initializeSection();
   initializeMenu();

   invoked = _menu->exec(widgie->mapToGlobal(point));
   Unit::unitDisplay unit = (Unit::unitDisplay)Brewken::option(propertyName, Unit::noUnit, _section, Brewken::UNIT).toInt();
   Unit::unitScale scale  = (Unit::unitScale)Brewken::option(propertyName, Unit::noUnit, _section, Brewken::SCALE).toInt();

   if ( invoked == 0 )
      return;

   QWidget* pMenu = invoked->parentWidget();
   if ( pMenu == _menu )
   {
      Brewken::setOption(propertyName, invoked->data(), _section, Brewken::UNIT);
      // reset the scale if required
      if ( Brewken::hasOption(propertyName, _section, Brewken::SCALE) )
         Brewken::setOption(propertyName, Unit::noScale, _section, Brewken::SCALE);
   }
   else
      Brewken::setOption(propertyName, invoked->data(), _section, Brewken::SCALE);

   // To make this all work, I need to set ogMin and ogMax when og is set.
   if ( propertyName == "og" )
   {
      Brewken::setOption(PropertyNames::Style::ogMin, invoked->data(),_section, Brewken::UNIT);
      Brewken::setOption(PropertyNames::Style::ogMax, invoked->data(),_section, Brewken::UNIT);
   }
   else if ( propertyName == "fg" )
   {
      Brewken::setOption(PropertyNames::Style::fgMin, invoked->data(),_section, Brewken::UNIT);
      Brewken::setOption(PropertyNames::Style::fgMax, invoked->data(),_section, Brewken::UNIT);
   }
   else if ( propertyName == "color_srm" )
   {
      Brewken::setOption(PropertyNames::Style::colorMin_srm, invoked->data(),_section, Brewken::UNIT);
      Brewken::setOption(PropertyNames::Style::colorMax_srm, invoked->data(),_section, Brewken::UNIT);
   }

   // Hmm. For the color fields, I want to include the ecb or srm in the label
   // text here.
   if ( whatAmI == COLOR )
   {
      Unit::unitDisplay disp = (Unit::unitDisplay)invoked->data().toInt();
      setText( tr("Color (%1)").arg(Brewken::colorUnitName(disp)));
   }

   // Remember, we need the original unit, not the new one.

   emit labelChanged(unit,scale);

}

BtColorLabel::BtColorLabel(QWidget *parent)
   : BtLabel(parent,COLOR)
{
}

BtDateLabel::BtDateLabel(QWidget *parent)
   : BtLabel(parent,DATE)
{
}

BtDensityLabel::BtDensityLabel(QWidget *parent)
   : BtLabel(parent,DENSITY)
{
}

BtMassLabel::BtMassLabel(QWidget *parent)
   : BtLabel(parent,MASS)
{
}

BtMixedLabel::BtMixedLabel(QWidget *parent)
   : BtLabel(parent,MIXED)
{
}

BtTemperatureLabel::BtTemperatureLabel(QWidget *parent)
   : BtLabel(parent,TEMPERATURE)
{
}

BtTimeLabel::BtTimeLabel(QWidget *parent)
   : BtLabel(parent,TIME)
{
}

BtVolumeLabel::BtVolumeLabel(QWidget *parent)
   : BtLabel(parent,VOLUME)
{
}

BtDiastaticPowerLabel::BtDiastaticPowerLabel(QWidget *parent)
   : BtLabel(parent,DIASTATIC_POWER)
{
}
