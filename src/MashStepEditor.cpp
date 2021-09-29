/*======================================================================================================================
 * MashStepEditor.cpp is part of Brewken, and is copyright the following authors 2009-2021:
 *   • Brian Rower <brian.rower@gmail.com>
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
#include "MashStepEditor.h"

#include "Brewken.h"
#include "model/MashStep.h"
#include "units/Unit.h"

MashStepEditor::MashStepEditor(QWidget* parent)
   : QDialog(parent), obs(nullptr)
{
   setupUi(this);

   comboBox_type->setCurrentIndex(-1);

   connect( buttonBox, &QDialogButtonBox::accepted, this, &MashStepEditor::saveAndClose );
   connect( buttonBox, &QDialogButtonBox::rejected, this, &MashStepEditor::close );
   connect( comboBox_type, SIGNAL(currentIndexChanged(const QString &)), this, SLOT(grayOutStuff(const QString &)) );

}

void MashStepEditor::showChanges(QMetaProperty* metaProp)
{
   if( obs == nullptr )
   {
      clear();
      return;
   }

   QString propName;
   QVariant value;
   bool updateAll = false;

   if( metaProp == nullptr )
      updateAll = true;
   else
   {
      propName = metaProp->name();
      value = metaProp->read(obs);
   }

   if ( updateAll )
   {
      lineEdit_name->setText(obs->name());
      comboBox_type->setCurrentIndex(obs->type());
      lineEdit_infuseAmount->setText(obs);
      lineEdit_infuseTemp->setText(obs);
      lineEdit_decoctionAmount->setText(obs);
      lineEdit_stepTemp->setText(obs);
      lineEdit_stepTime->setText(obs);
      lineEdit_rampTime->setText(obs);
      lineEdit_endTemp->setText(obs);
   }

   else if( propName == PropertyNames::NamedEntity::name )
      lineEdit_name->setText(obs->name());
   else if( propName == "type" )
      comboBox_type->setCurrentIndex(obs->type());
   else if( propName == PropertyNames::MashStep::infuseAmount_l )
      lineEdit_infuseAmount->setText(obs);
   else if( propName == PropertyNames::MashStep::infuseTemp_c )
      lineEdit_infuseTemp->setText(obs);
   else if( propName == PropertyNames::MashStep::decoctionAmount_l )
      lineEdit_decoctionAmount->setText(obs);
   else if( propName == PropertyNames::MashStep::stepTemp_c )
      lineEdit_stepTemp->setText(obs);
   else if( propName == PropertyNames::MashStep::stepTime_min )
      lineEdit_stepTime->setText(obs);
   else if( propName == PropertyNames::MashStep::rampTime_min )
      lineEdit_rampTime->setText(obs);
   else if( propName == PropertyNames::MashStep::endTemp_c )
      lineEdit_endTemp->setText(obs);
}

void MashStepEditor::clear()
{
   lineEdit_name->setText(QString(""));
   comboBox_type->setCurrentIndex(0);
   lineEdit_infuseAmount->setText(QString(""));
   lineEdit_infuseTemp->setText(QString(""));
   lineEdit_decoctionAmount->setText(QString(""));
   lineEdit_stepTemp->setText(QString(""));
   lineEdit_stepTime->setText(QString(""));
   lineEdit_rampTime->setText(QString(""));
   lineEdit_endTemp->setText(QString(""));
}

void MashStepEditor::close()
{
   setVisible(false);
}

void MashStepEditor::changed(QMetaProperty prop, QVariant /*val*/)
{
   if( sender() != obs )
      return;

   showChanges(&prop);
}

void MashStepEditor::setMashStep(MashStep* step) {
   if (this->obs) {
      disconnect(this->obs, nullptr, this, nullptr );
   }

   if (step) {
      this->obs = step;
      connect(this->obs, SIGNAL(changed(QMetaProperty,QVariant)), this, SLOT(changed(QMetaProperty,QVariant)) );
      showChanges();
   }
}

void MashStepEditor::saveAndClose() {
   this->obs->setName(lineEdit_name->text());
   this->obs->setType(static_cast<MashStep::Type>(comboBox_type->currentIndex()));
   this->obs->setInfuseAmount_l(lineEdit_infuseAmount->toSI());
   this->obs->setInfuseTemp_c(lineEdit_infuseTemp->toSI());
   this->obs->setDecoctionAmount_l(lineEdit_decoctionAmount->toSI());
   this->obs->setStepTemp_c(lineEdit_stepTemp->toSI());
   this->obs->setStepTime_min(lineEdit_stepTime->toSI());
   this->obs->setRampTime_min(lineEdit_rampTime->toSI());
   this->obs->setEndTemp_c(lineEdit_endTemp->toSI());

   if (this->obs->key() < 0) {
      // This is a new MashStep, so we need to store it.
      // We'll ask MainWindow to do this for us, because then it can be an undoable action.
      //
      // The Mash of this MashStep should already have been set by the caller
      Brewken::mainWindow()->addMashStepToMash(this->obs);
   }

   setVisible(false);
   return;
}

void MashStepEditor::grayOutStuff(const QString& text)
{
   if( text == "Infusion" )
   {
      lineEdit_infuseAmount->setEnabled(true);
      lineEdit_infuseTemp->setEnabled(true);
      lineEdit_decoctionAmount->setEnabled(false);
   }
   else if( text == "Decoction" )
   {
      lineEdit_infuseAmount->setEnabled(false);
      lineEdit_infuseTemp->setEnabled(false);
      lineEdit_decoctionAmount->setEnabled(true);
   }
   else if( text == "Temperature" )
   {
      lineEdit_infuseAmount->setEnabled(false);
      lineEdit_infuseTemp->setEnabled(false);
      lineEdit_decoctionAmount->setEnabled(false);
   }
   else
   {
      lineEdit_infuseAmount->setEnabled(true);
      lineEdit_infuseTemp->setEnabled(true);
      lineEdit_decoctionAmount->setEnabled(true);
   }
}
