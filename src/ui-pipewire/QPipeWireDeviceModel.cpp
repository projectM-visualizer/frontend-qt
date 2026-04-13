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
#include "QPipeWireThread.hpp"
#include <QColor>
#include <algorithm>

QPipeWireDeviceModel::QPipeWireDeviceModel(const QHash<uint32_t, QString> & devices,
                                           uint32_t currentNodeId,
                                           QObject * parent)
    : QAbstractListModel(parent), m_devices(devices), m_currentNodeId(currentNodeId)
{
    refreshKeys();
}

void QPipeWireDeviceModel::refreshKeys()
{
    m_sortedKeys = m_devices.keys();
    std::sort(m_sortedKeys.begin(), m_sortedKeys.end());
}

void QPipeWireDeviceModel::updateItemHighlights()
{
    m_currentNodeId = QPipeWireThread::currentNodeId();
    beginResetModel();
    refreshKeys();
    endResetModel();
}

void QPipeWireDeviceModel::setCurrentNodeId(uint32_t nodeId)
{
    m_currentNodeId = nodeId;
    updateItemHighlights();
}

uint32_t QPipeWireDeviceModel::nodeIdForRow(int row) const
{
    if (row < 0 || row >= m_sortedKeys.size())
        return 0;
    return m_sortedKeys[row];
}

QVariant QPipeWireDeviceModel::data(const QModelIndex & index, int role) const
{
    if (!index.isValid() || index.row() >= m_sortedKeys.size())
        return QVariant();

    uint32_t nodeId = m_sortedKeys[index.row()];
    QString deviceName = m_devices[nodeId];

    switch (role)
    {
        case Qt::DisplayRole:
            return deviceName;

        case Qt::DecorationRole:
            return QVariant();

        case Qt::ToolTipRole:
            if (nodeId == m_currentNodeId)
                return deviceName + " (active)";
            else
                return deviceName + " (inactive)";

        case Qt::BackgroundRole:
            if (nodeId == m_currentNodeId) {
                QColor highlight(0, 200, 0, 80);
                return highlight;
            }
            return QVariant();

        default:
            return QVariant();
    }
}

int QPipeWireDeviceModel::rowCount(const QModelIndex & parent) const
{
    Q_UNUSED(parent);
    return m_sortedKeys.size();
}
