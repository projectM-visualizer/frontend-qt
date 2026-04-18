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

#include "QAudioDeviceModel.hpp"
#include "QAudioBackend.hpp"

#include <QColor>

QAudioDeviceModel::QAudioDeviceModel(QAudioBackend *backend, QObject *parent)
    : QAbstractListModel(parent), m_backend(backend)
{
    connect(m_backend, SIGNAL(devicesChanged()), this, SLOT(refresh()));
    connect(m_backend, SIGNAL(activeDeviceChanged()), this, SLOT(refresh()));
}

QAudioDeviceModel::~QAudioDeviceModel()
{
}

void QAudioDeviceModel::refresh()
{
    beginResetModel();
    endResetModel();
}

int QAudioDeviceModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return m_backend->devices().size();
}

QVariant QAudioDeviceModel::data(const QModelIndex &index, int role) const
{
    QList<QAudioBackend::DeviceInfo> devs = m_backend->devices();

    if (!index.isValid() || index.row() >= devs.size())
        return QVariant();

    const QAudioBackend::DeviceInfo &info = devs[index.row()];

    switch (role)
    {
        case Qt::DisplayRole:
            return info.displayName;

        case Qt::ToolTipRole:
            if (info.id == m_backend->currentDeviceId())
                return info.displayName + " (active)";
            else
                return info.displayName;

        case Qt::BackgroundRole:
            if (info.id == m_backend->currentDeviceId()) {
                QColor highlight(0, 200, 0, 80);
                return highlight;
            }
            return QVariant();

        case DeviceIdRole:
            return info.id;

        default:
            return QVariant();
    }
}
