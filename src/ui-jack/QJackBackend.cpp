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

#include "QJackBackend.hpp"
#include "qprojectm_mainwindow.hpp"

#include <QtDebug>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <cstdio>
#include <memory>
#include <mutex>
#include <thread>

QJackBackend::QJackBackend(QObject *parent)
    : QAudioBackend(parent)
    , m_client(nullptr)
    , m_inputPort(nullptr)
    , m_mainWindow(nullptr)
{
}

QJackBackend::~QJackBackend()
{
    stop();
}

bool QJackBackend::start(QProjectM_MainWindow *mainWindow, QMutex *audioMutex)
{
    Q_UNUSED(audioMutex);

    if (m_client) {
        // Already running; reactivate (idempotent for backend caching).
        m_mainWindow = mainWindow;
        m_active = true;
        return true;
    }

    m_mainWindow = mainWindow;

    // Open a client connection to the JACK server.
    // Run in a worker thread with a timeout — jack_client_open can block
    // for several seconds even with JackNoStartServer if the server socket
    // exists but is unresponsive.
    //
    // The state is heap-allocated and shared with the worker so it outlives
    // a timeout. On timeout we mark the call as abandoned and detach the
    // thread; if the call eventually returns a valid client, the worker
    // closes it itself. This avoids QThread::terminate (async, unsafe) and
    // dangling references to stack variables.
    struct OpenState {
        std::mutex mutex;
        std::condition_variable cv;
        jack_client_t *client = nullptr;
        jack_status_t status = JackFailure;
        bool done = false;
        bool abandoned = false;
    };

    auto state = std::make_shared<OpenState>();

    std::thread worker([state]() {
        jack_status_t localStatus = JackFailure;
        jack_client_t *result = jack_client_open("projectM", JackNoStartServer, &localStatus, nullptr);

        bool shouldClose = false;
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            state->status = localStatus;
            if (state->abandoned) {
                shouldClose = (result != nullptr);
            } else {
                state->client = result;
            }
            state->done = true;
        }
        state->cv.notify_all();

        // Caller gave up before we returned; clean up the orphan client.
        if (shouldClose) {
            jack_client_close(result);
        }
    });
    worker.detach();

    {
        std::unique_lock<std::mutex> lock(state->mutex);
        if (!state->cv.wait_for(lock, std::chrono::seconds(3),
                                [&]{ return state->done; })) {
            state->abandoned = true;
            qWarning() << "JACK connection timed out";
            emit errorOccurred(QStringLiteral("JACK server connection timed out"));
            return false;
        }
        m_client = state->client;
        if (!m_client) {
            qCritical() << "jack_client_open() failed, status =" << state->status;
            emit errorOccurred(QStringLiteral("Failed to connect to JACK server"));
            return false;
        }
    }

    if (state->status & JackNameNotUnique) {
        qDebug() << "JACK: unique name assigned:" << jack_get_client_name(m_client);
    }

    // Set the process callback -- JACK calls this from its own realtime thread
    if (jack_set_process_callback(m_client, processCallback, this)) {
        qCritical() << "Failed to set JACK process callback";
        jack_client_close(m_client);
        m_client = nullptr;
        emit errorOccurred(QStringLiteral("Failed to set JACK process callback"));
        return false;
    }

    // Register a shutdown callback
    jack_on_shutdown(m_client, shutdownCallback, this);

    qDebug() << "JACK engine sample rate:" << jack_get_sample_rate(m_client);

    // Register the input port
    m_inputPort = jack_port_register(m_client, "input",
                                     JACK_DEFAULT_AUDIO_TYPE,
                                     JackPortIsInput, 0);
    if (!m_inputPort) {
        qCritical() << "No more JACK ports available";
        jack_client_close(m_client);
        m_client = nullptr;
        emit errorOccurred(QStringLiteral("Failed to register JACK input port"));
        return false;
    }

    // Activate the client -- the process callback starts running
    if (jack_activate(m_client)) {
        qCritical() << "Cannot activate JACK client";
        jack_client_close(m_client);
        m_client = nullptr;
        m_inputPort = nullptr;
        emit errorOccurred(QStringLiteral("Failed to activate JACK client"));
        return false;
    }

    // Enumerate all output ports and connect them to our input
    const char **ports = jack_get_ports(m_client, nullptr, nullptr, JackPortIsOutput);
    if (ports) {
        for (int i = 0; ports[i] != nullptr; ++i) {
            qDebug() << "Connecting to JACK port" << ports[i];
            if (jack_connect(m_client, ports[i], jack_port_name(m_inputPort))) {
                qWarning() << "Cannot connect JACK port" << ports[i];
            }
        }
        jack_free(ports);
    } else {
        qWarning() << "No physical capture ports found in JACK";
    }

    return true;
}

void QJackBackend::stop()
{
    if (!m_client) {
        return;
    }

    jack_deactivate(m_client);
    jack_client_close(m_client);
    m_client = nullptr;
    m_inputPort = nullptr;
}

int QJackBackend::processCallback(jack_nframes_t nframes, void *arg)
{
    QJackBackend *self = static_cast<QJackBackend *>(arg);

    jack_default_audio_sample_t *in =
        static_cast<jack_default_audio_sample_t *>(
            jack_port_get_buffer(self->m_inputPort, nframes));

    if (in && self->m_mainWindow) {
        self->m_mainWindow->addPCM(in, nframes);
    }

    return 0;
}

void QJackBackend::shutdownCallback(void *arg)
{
    Q_UNUSED(arg);
    qWarning() << "JACK server shut down";
}

QString QJackBackend::backendName() const
{
    return QStringLiteral("JACK");
}

QList<QAudioBackend::DeviceInfo> QJackBackend::devices() const
{
    QList<DeviceInfo> result;
    DeviceInfo info;
    info.id = QStringLiteral("jack-auto");
    info.displayName = QStringLiteral("JACK (all ports, auto-connect)");
    result.append(info);
    return result;
}

QString QJackBackend::currentDeviceId() const
{
    return QStringLiteral("jack-auto");
}

bool QJackBackend::supportsDeviceSwitching() const
{
    return false;
}

void QJackBackend::selectDevice(const QString &deviceId)
{
    Q_UNUSED(deviceId);
    // JACK auto-connects to all output ports; device selection is a no-op.
}

void QJackBackend::writeSettings()
{
    // No persistent settings for JACK backend.
}

void QJackBackend::readSettings()
{
    // No persistent settings for JACK backend.
}
