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

#ifndef QAUDIO_DEVICE_MODEL_HPP
#define QAUDIO_DEVICE_MODEL_HPP

#include <QAbstractListModel>

class QAudioBackend;

class QAudioDeviceModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Roles {
        DeviceIdRole = Qt::UserRole + 1
    };

    explicit QAudioDeviceModel(QAudioBackend *backend, QObject *parent = nullptr);
    ~QAudioDeviceModel();

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;

public slots:
    void refresh();

private:
    QAudioBackend *m_backend;
};

#endif // QAUDIO_DEVICE_MODEL_HPP
