/*======================================================================================================================
 * sortFilterProxyModels/BtTreeFilterProxyModel.h is part of Brewken, and is copyright the following authors 2009-2024:
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
#ifndef SORTFILTERPROXYMODELS_BTTREEFILTERPROXYMODEL_H
#define SORTFILTERPROXYMODELS_BTTREEFILTERPROXYMODEL_H
#pragma once

#include <QModelIndex>
#include <QSortFilterProxyModel>

#include "BtTreeModel.h"

/*!
 * \class BtTreeFilterProxyModel
 *
 * \brief Proxy model for sorting Brewken trees.
 */
class BtTreeFilterProxyModel : public QSortFilterProxyModel {
   Q_OBJECT

public:
   BtTreeFilterProxyModel(QObject *parent, BtTreeModel::TypeMasks mask);

protected:
   bool lessThan(const QModelIndex &left, const QModelIndex &right) const;
   bool filterAcceptsRow( int source_row, const QModelIndex &source_parent) const;

private:
   BtTreeModel::TypeMasks m_treeMask;
};

#endif
