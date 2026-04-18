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

#include "QPulseAudioBackend.hpp"
#include "QPulseAudioThread.hpp"

#include <QMutex>
#include <QModelIndex>
#include <QStandardItemModel>
#include <QList>
#include <algorithm>

QPulseAudioBackend::QPulseAudioBackend(QObject *parent)
    : QAudioBackend(parent)
    , m_thread(nullptr)
    , m_mainWindow(nullptr)
{
}

QPulseAudioBackend::~QPulseAudioBackend()
{
    stop();
}

bool QPulseAudioBackend::start(QProjectM_MainWindow *mainWindow, QMutex *audioMutex)
{
    Q_UNUSED(audioMutex);

    if (m_thread) {
        return true; // already running, reuse
    }

    m_mainWindow = mainWindow;

    // QPulseAudioThread requires argc/argv; pass 0/nullptr since the unified
    // main handles argument parsing before the backend is created.
    m_thread = new QPulseAudioThread(0, nullptr, mainWindow);

    connect(m_thread, &QPulseAudioThread::deviceChanged,
            this, &QPulseAudioBackend::activeDeviceChanged);

    m_thread->start();
    return true;
}

void QPulseAudioBackend::stop()
{
    // Keep the thread alive — PulseAudio has similar re-init issues.
    // The thread stays running but audio isn't consumed while another
    // backend is active.
}

QString QPulseAudioBackend::backendName() const
{
    return QStringLiteral("PulseAudio");
}

QList<QAudioBackend::DeviceInfo> QPulseAudioBackend::devices() const
{
    QList<DeviceInfo> result;

    if (!m_thread) {
        return result;
    }

    const QPulseAudioThread::SourceContainer &sources = m_thread->devices();
    QPulseAudioThread::SourceContainer::const_iterator it;
    for (it = sources.constBegin(); it != sources.constEnd(); ++it) {
        DeviceInfo info;
        info.id = QString::number(it.key());
        info.displayName = it.value();
        result.append(info);
    }

    return result;
}

QString QPulseAudioBackend::currentDeviceId() const
{
    if (!m_thread) {
        return QString();
    }

    QPulseAudioThread::SourceContainer::const_iterator pos = m_thread->sourcePosition();
    const QPulseAudioThread::SourceContainer &sources = m_thread->devices();

    if (pos != sources.constEnd()) {
        return QString::number(pos.key());
    }

    return QString();
}

bool QPulseAudioBackend::supportsDeviceSwitching() const
{
    return true;
}

void QPulseAudioBackend::selectDevice(const QString &deviceId)
{
    if (!m_thread) {
        return;
    }

    // QPulseAudioThread::connectDevice() uses index.row() as the key for
    // s_sourceList.find(index.row()).  We need to produce a QModelIndex whose
    // row() equals the PulseAudio source index (the int key in the hash).
    //
    // QModelIndex can only be created via a model.  We use a temporary
    // QStandardItemModel with enough rows to cover the target key value, then
    // pull out the index at the right row.
    int targetKey = deviceId.toInt();
    const QPulseAudioThread::SourceContainer &sources = m_thread->devices();

    if (!sources.contains(targetKey)) {
        return;
    }

    // Create a temporary model large enough to produce a valid index at
    // row == targetKey.
    QStandardItemModel tmpModel(targetKey + 1, 1);
    QModelIndex idx = tmpModel.index(targetKey, 0);
    m_thread->connectDevice(idx);
}

void QPulseAudioBackend::writeSettings()
{
    if (m_thread) {
        m_thread->writeSettings();
    }
}

void QPulseAudioBackend::readSettings()
{
    // PulseAudio readSettings is a private static method on the thread;
    // it is called internally during initialization.
}
