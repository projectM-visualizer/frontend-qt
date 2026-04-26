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

#include "QPipeWireBackend.hpp"
#include "QPipeWireThread.hpp"

#include <QMutex>
#include <QHashIterator>

QPipeWireBackend::QPipeWireBackend(QObject *parent)
    : QAudioBackend(parent)
    , m_thread(nullptr)
    , m_mainWindow(nullptr)
    , m_argc(0)
{
    m_argv[0] = nullptr;
}

QPipeWireBackend::~QPipeWireBackend()
{
    // Real teardown happens here at process exit, not on stop() — stop()
    // only deactivates audio because PipeWire can't be safely re-initialized
    // within the same process.
    if (m_thread) {
        QPipeWireThread::setAudioActive(false);
        m_thread->writeSettings();
        m_thread->cleanup(); // quits main loop, waits for thread, destroys PW resources
        delete m_thread;
        m_thread = nullptr;
    }
}

bool QPipeWireBackend::start(QProjectM_MainWindow *mainWindow, QMutex *audioMutex)
{
    if (m_thread) {
        return true; // already running, reuse
    }

    m_mainWindow = mainWindow;

    QPipeWireThread::setAudioMutex(audioMutex);
    m_thread = new QPipeWireThread(m_argc, m_argv, mainWindow);

    connect(m_thread, &QPipeWireThread::deviceChanged,
            this, &QPipeWireBackend::activeDeviceChanged);

    m_thread->start();
    m_active = true;
    QPipeWireThread::setAudioActive(true);
    return true;
}

void QPipeWireBackend::stop()
{
    // Keep the thread alive — PipeWire cannot be re-initialized after
    // cleanup within the same process. Pause audio delivery instead so
    // samples aren't double-fed through addPCM when another backend
    // becomes active.
    m_active = false;
    QPipeWireThread::setAudioActive(false);
}

QString QPipeWireBackend::backendName() const
{
    return QStringLiteral("PipeWire");
}

QList<QAudioBackend::DeviceInfo> QPipeWireBackend::devices() const
{
    QList<DeviceInfo> result;
    const QHash<uint32_t, QString> &sources = QPipeWireThread::sourceList();

    QHashIterator<uint32_t, QString> it(sources);
    while (it.hasNext()) {
        it.next();
        DeviceInfo info;
        info.id = QString::number(it.key());
        info.displayName = it.value();
        result.append(info);
    }

    return result;
}

QString QPipeWireBackend::currentDeviceId() const
{
    return QString::number(QPipeWireThread::currentNodeId());
}

bool QPipeWireBackend::supportsDeviceSwitching() const
{
    return true;
}

void QPipeWireBackend::selectDevice(const QString &deviceId)
{
    if (m_thread) {
        m_thread->connectDeviceById(deviceId.toUInt());
    }
}

void QPipeWireBackend::writeSettings()
{
    if (m_thread) {
        m_thread->writeSettings();
    }
}

void QPipeWireBackend::readSettings()
{
    if (m_thread) {
        m_thread->readSettings();
    }
}
