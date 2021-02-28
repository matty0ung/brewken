/**
 * WaterTableWidget.h is part of Brewken, and is copyright the following authors 2009-2014:
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
#ifndef WATERTABLEWIDGET_H
#define WATERTABLEWIDGET_H

class WaterTableWidget;

#include <QTableView>
#include <QWidget>
class WaterTableModel;

/*!
 * \class WaterTableWidget
 *
 *
 * \brief Completely redundant class. Remove and just use QTableView.
 */
class WaterTableWidget : public QTableView
{
   Q_OBJECT

public:
   WaterTableWidget(QWidget* parent=0);
   WaterTableModel* getModel();

private:
   WaterTableModel* model;
};

#endif   //WATERTABLEWIDGET_H