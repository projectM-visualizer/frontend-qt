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

#ifndef QPIPEWIREDEVICEMODEL_HPP
#define QPIPEWIREDEVICEMODEL_HPP

#include <QAbstractListModel>
#include <QHash>
#include <QList>
#include <cstdint>

class QPipeWireDeviceModel : public QAbstractListModel
{
    Q_OBJECT

public slots:
    void updateItemHighlights();

public:
    QPipeWireDeviceModel(const QHash<uint32_t, QString> & devices, uint32_t currentNodeId, QObject * parent);
    ~QPipeWireDeviceModel() { }

    QVariant data(const QModelIndex & index, int role) const;
    int rowCount(const QModelIndex & parent = QModelIndex()) const;

    void setCurrentNodeId(uint32_t nodeId);

    uint32_t nodeIdForRow(int row) const;

private:
    void refreshKeys();

    const QHash<uint32_t, QString> & m_devices;
    QList<uint32_t> m_sortedKeys;
    uint32_t m_currentNodeId;
};

#endif
