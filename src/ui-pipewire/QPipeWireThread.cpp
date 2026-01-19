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

#include "QPipeWireThread.hpp"
#include <QSettings>
#include <cstring>

QMutex *QPipeWireThread::s_audioMutex = nullptr;
QPipeWireThread::AudioData QPipeWireThread::s_data = {};

QPipeWireThread::QPipeWireThread(int _argc, char **_argv, QProjectM_MainWindow *mainWindow)
    : QThread(nullptr), argc(_argc), argv(_argv), m_qprojectM_MainWindow(mainWindow)
{
}

QPipeWireThread::~QPipeWireThread()
{
}

QMutex *QPipeWireThread::mutex()
{
    return s_audioMutex;
}

void QPipeWireThread::writeSettings()
{
    // Settings are minimal for PipeWire - it auto-connects
    QSettings settings("projectM", "qprojectM-pipewire");
}

void QPipeWireThread::on_state_changed(void *data, enum pw_stream_state old_state,
                                        enum pw_stream_state state, const char *error)
{
    Q_UNUSED(old_state);
    AudioData *audioData = static_cast<AudioData *>(data);

    switch (state) {
    case PW_STREAM_STATE_ERROR:
        qCritical() << "PipeWire stream error:" << error;
        break;
    case PW_STREAM_STATE_PAUSED:
        qDebug() << "PipeWire stream paused";
        break;
    case PW_STREAM_STATE_STREAMING:
        qDebug() << "PipeWire stream streaming";
        break;
    case PW_STREAM_STATE_UNCONNECTED:
        qDebug() << "PipeWire stream unconnected";
        break;
    case PW_STREAM_STATE_CONNECTING:
        qDebug() << "PipeWire stream connecting";
        break;
    }
}

void QPipeWireThread::on_process(void *userdata)
{
    AudioData *data = static_cast<AudioData *>(userdata);

    if (!data->mainWindow) {
        return;
    }

    struct pw_buffer *b;
    struct spa_buffer *buf;
    float *samples;
    uint32_t n_samples;

    b = pw_stream_dequeue_buffer(data->stream);
    if (b == nullptr) {
        qWarning() << "PipeWire: out of buffers";
        return;
    }

    buf = b->buffer;
    if (buf->datas[0].data == nullptr) {
        return;
    }

    samples = static_cast<float *>(buf->datas[0].data);
    n_samples = buf->datas[0].chunk->size / sizeof(float);

    if (data->audioMutex) {
        data->audioMutex->lock();
    }

    // Add audio samples to projectM
    data->mainWindow->addPCM(samples, n_samples);

    if (data->audioMutex) {
        data->audioMutex->unlock();
    }

    pw_stream_queue_buffer(data->stream, b);
}

void QPipeWireThread::cleanup()
{
    if (s_data.stream) {
        pw_stream_destroy(s_data.stream);
        s_data.stream = nullptr;
    }

    if (s_data.loop) {
        pw_main_loop_quit(s_data.loop);
        pw_main_loop_destroy(s_data.loop);
        s_data.loop = nullptr;
    }

    pw_deinit();
}

void QPipeWireThread::run()
{
    const struct pw_stream_events stream_events = {
        .version = PW_VERSION_STREAM_EVENTS,
        .state_changed = on_state_changed,
        .process = on_process,
    };

    struct spa_pod_builder b = SPA_POD_BUILDER_INIT(nullptr, 0);
    const struct spa_pod *params[1];
    uint8_t buffer[1024];

    pw_init(&argc, &argv);

    s_data.loop = pw_main_loop_new(nullptr);
    s_data.mainWindow = m_qprojectM_MainWindow;
    s_data.audioMutex = s_audioMutex;

    if (!s_data.loop) {
        qCritical() << "Failed to create PipeWire main loop";
        emit threadCleanedUp();
        return;
    }

    pw_properties *props = pw_properties_new(
        PW_KEY_MEDIA_TYPE, "Audio",
        PW_KEY_MEDIA_CATEGORY, "Capture",
        PW_KEY_MEDIA_ROLE, "Music",
        PW_KEY_APP_NAME, "projectM",
        nullptr);

    s_data.stream = pw_stream_new_simple(
        pw_main_loop_get_loop(s_data.loop),
        "projectM-audio-capture",
        props,
        &stream_events,
        &s_data);

    if (!s_data.stream) {
        qCritical() << "Failed to create PipeWire stream";
        pw_main_loop_destroy(s_data.loop);
        emit threadCleanedUp();
        return;
    }

    // Set up audio format: 32-bit float, stereo, 44.1kHz
    b = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));

    struct spa_audio_info_raw audio_info = {
        .format = SPA_AUDIO_FORMAT_F32,
        .flags = SPA_AUDIO_FLAG_NONE,
        .rate = 44100,
        .channels = 2,
    };
    audio_info.position[0] = SPA_AUDIO_CHANNEL_FL;
    audio_info.position[1] = SPA_AUDIO_CHANNEL_FR;

    params[0] = spa_format_audio_raw_build(&b, SPA_PARAM_EnumFormat, &audio_info);

    // Connect to default source with auto-connect
    int ret = pw_stream_connect(s_data.stream,
                                 PW_DIRECTION_INPUT,
                                 PW_ID_ANY,
                                 static_cast<pw_stream_flags>(
                                     PW_STREAM_FLAG_AUTOCONNECT |
                                     PW_STREAM_FLAG_MAP_BUFFERS |
                                     PW_STREAM_FLAG_RT_PROCESS),
                                 params, 1);

    if (ret < 0) {
        qCritical() << "Failed to connect PipeWire stream:" << strerror(-ret);
        pw_stream_destroy(s_data.stream);
        pw_main_loop_destroy(s_data.loop);
        emit threadCleanedUp();
        return;
    }

    qDebug() << "PipeWire stream started successfully";

    // Run the main loop
    pw_main_loop_run(s_data.loop);

    emit threadCleanedUp();
}
