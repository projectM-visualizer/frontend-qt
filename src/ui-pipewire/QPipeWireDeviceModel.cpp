/**
 * projectM -- Milkdrop-esque visualisation SDK
 * Copyright (C)2003-2004 projectM Team
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 * See 'LICENSE.txt' included within this release
 *
 */

#include "QPipeWireDeviceModel.hpp"
#include <QtWidgets>
#include <QIcon>
#include <QtDebug>

QPipeWireDeviceModel::QPipeWireDeviceModel(const QHash<uint32_t, QString> & devices,
                                           uint32_t currentNodeId,
                                           QObject * parent)
    : QAbstractListModel(parent), m_devices(devices), m_currentNodeId(currentNodeId)
{
}

void QPipeWireDeviceModel::updateItemHighlights()
{
    if (rowCount() == 0)
        return;

    emit dataChanged(this->index(0), this->index(rowCount()-1));
}

void QPipeWireDeviceModel::setCurrentNodeId(uint32_t nodeId)
{
    m_currentNodeId = nodeId;
    updateItemHighlights();
}

QVariant QPipeWireDeviceModel::data(const QModelIndex & index, int role) const
{
    if (!index.isValid())
        return QVariant();

    if (index.row() >= rowCount())
        return QVariant();

    // Get the node ID for this row
    QList<uint32_t> keys = m_devices.keys();
    if (index.row() >= keys.size())
        return QVariant();

    uint32_t nodeId = keys[index.row()];
    QString deviceName = m_devices[nodeId];

    switch (role)
    {
        case Qt::DisplayRole:
            return deviceName;

        case Qt::DecorationRole:
        {
            if (nodeId == m_currentNodeId) {
                QIcon icon(":/check.png");
                return icon;
            }
            return QVariant();
        }

        case Qt::ToolTipRole:
            if (nodeId == m_currentNodeId)
                return deviceName + " (active)";
            else
                return deviceName + " (inactive)";

        case Qt::BackgroundRole:
            if (nodeId == m_currentNodeId) {
                // Use a semi-transparent green that works on both light and dark themes
                QColor highlight(0, 200, 0, 80);  // Green with 80/255 alpha
                return highlight;
            }
            // Don't set background for inactive items - let theme handle it
            return QVariant();

        default:
            return QVariant();
    }
}

int QPipeWireDeviceModel::rowCount(const QModelIndex & parent) const
{
    Q_UNUSED(parent);
    return m_devices.count();
}
