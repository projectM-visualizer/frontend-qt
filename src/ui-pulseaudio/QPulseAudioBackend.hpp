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

#ifndef QPULSEAUDIO_BACKEND_HPP
#define QPULSEAUDIO_BACKEND_HPP

#include "QAudioBackend.hpp"

class QPulseAudioThread;

class QPulseAudioBackend : public QAudioBackend
{
    Q_OBJECT

public:
    explicit QPulseAudioBackend(QObject *parent = nullptr);
    virtual ~QPulseAudioBackend();

    bool start(QProjectM_MainWindow *mainWindow, QMutex *audioMutex) override;
    void stop() override;

    QString backendName() const override;

    QList<DeviceInfo> devices() const override;
    QString currentDeviceId() const override;
    bool supportsDeviceSwitching() const override;

    void writeSettings() override;
    void readSettings() override;

public slots:
    void selectDevice(const QString &deviceId) override;

private:
    QPulseAudioThread *m_thread;
    QProjectM_MainWindow *m_mainWindow;
};

#endif // QPULSEAUDIO_BACKEND_HPP
