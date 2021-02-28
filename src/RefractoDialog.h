/**
 * RefractoDialog.h is part of Brewken, and is copyright the following authors 2009-2014:
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

#ifndef _REFRACTODIALOG_H
#define _REFRACTODIALOG_H

class RefractoDialog;

#include <QDialog>
#include <QWidget>
#include "ui_refractoDialog.h"

/*!
 * \class RefractoDialog
 *
 *
 * \brief Dialog for calculating refractometer stuff.
 */
class RefractoDialog : public QDialog, public Ui::refractoDialog
{
   Q_OBJECT
public:
   RefractoDialog(QWidget* parent = 0);
   ~RefractoDialog();

private slots:
   void calculate();

private:
   void clearOutputFields();
};

#endif /*_REFRACTODIALOG_H*/