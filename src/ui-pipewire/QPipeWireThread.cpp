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
#include <algorithm>

QMutex *QPipeWireThread::s_audioMutex = nullptr;
QPipeWireThread::AudioData QPipeWireThread::s_data = {};
struct pw_context *QPipeWireThread::s_context = nullptr;
QHash<uint32_t, QString> QPipeWireThread::s_sourceList;
QHash<uint32_t, bool> QPipeWireThread::s_isSinkMap;
QString QPipeWireThread::s_currentDeviceName;
uint32_t QPipeWireThread::s_currentNodeId = PW_ID_ANY;
std::atomic<bool> QPipeWireThread::s_audioActive{true};

void QPipeWireThread::setAudioActive(bool active)
{
    s_audioActive.store(active, std::memory_order_release);
}

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
    QSettings settings("projectM", "qprojectM-pipewire");
    settings.setValue("pipeWireNodeId", s_currentNodeId);
}

void QPipeWireThread::on_state_changed(void *data, enum pw_stream_state old_state,
                                        enum pw_stream_state state, const char *error)
{
    Q_UNUSED(old_state);
    Q_UNUSED(data);

    switch (state) {
    case PW_STREAM_STATE_ERROR:
        qCritical() << "PipeWire stream error:" << error;
        break;
    default:
        break;
    }
}

void QPipeWireThread::on_process(void *userdata)
{
    AudioData *data = static_cast<AudioData *>(userdata);

    if (!data->mainWindow) {
        return;
    }

    struct pw_buffer *b = pw_stream_dequeue_buffer(data->stream);
    if (b == nullptr) {
        return;
    }

    struct spa_buffer *buf = b->buffer;
    if (buf->datas[0].data == nullptr) {
        pw_stream_queue_buffer(data->stream, b);
        return;
    }

    // Drop audio when this backend isn't the active one (e.g., user switched
    // to another backend in the unified app). The stream stays connected
    // because PipeWire can't be re-initialized within the same process.
    if (!s_audioActive.load(std::memory_order_acquire)) {
        pw_stream_queue_buffer(data->stream, b);
        return;
    }

    float *samples = static_cast<float *>(buf->datas[0].data);
    uint32_t n_samples = buf->datas[0].chunk->size / sizeof(float);

    data->mainWindow->addPCM(samples, n_samples);

    pw_stream_queue_buffer(data->stream, b);
}

void QPipeWireThread::on_registry_global(void *data, uint32_t id,
                                         uint32_t permissions,
                                         const char *type,
                                         uint32_t version,
                                         const struct spa_dict *props)
{
    Q_UNUSED(permissions);
    Q_UNUSED(version);
    Q_UNUSED(data);

    if (!props) {
        return;
    }

    // Filter for Node interface only
    if (strcmp(type, PW_TYPE_INTERFACE_Node) != 0) {
        return;
    }

    // Get media class - we want Audio/Source (microphones, line-in) and Audio/Sink (for monitoring output)
    const char *media_class = spa_dict_lookup(props, PW_KEY_MEDIA_CLASS);
    if (!media_class) {
        return;
    }

    bool is_source = (strcmp(media_class, "Audio/Source") == 0);
    bool is_sink = (strcmp(media_class, "Audio/Sink") == 0);

    if (!is_source && !is_sink) {
        return;
    }

    // Extract human-readable name (prefer description > nick > name)
    const char *desc = spa_dict_lookup(props, PW_KEY_NODE_DESCRIPTION);
    const char *nick = spa_dict_lookup(props, PW_KEY_NODE_NICK);
    const char *name = spa_dict_lookup(props, PW_KEY_NODE_NAME);

    QString display_name = desc ? desc : (nick ? nick : (name ? name : "Unknown"));

    // Sinks are output devices - we can monitor/capture their audio
    // Mark them as [Monitor] to indicate they capture system audio output
    if (is_sink) {
        display_name = "[Monitor] " + display_name;
    }

    s_sourceList.insert(id, display_name);
    s_isSinkMap.insert(id, is_sink);
}

void QPipeWireThread::on_registry_global_remove(void *data, uint32_t id)
{
    Q_UNUSED(data);
    s_sourceList.remove(id);
    s_isSinkMap.remove(id);

    if (s_currentNodeId == id) {
        s_currentNodeId = PW_ID_ANY;
        s_currentDeviceName.clear();
    }
}

void QPipeWireThread::enumerateDevices()
{
    if (!s_data.core) {
        qWarning() << "Cannot enumerate devices: core not initialized";
        return;
    }

    // Get registry from core
    s_data.registry = pw_core_get_registry(s_data.core, PW_VERSION_REGISTRY, 0);

    // Setup registry events
    static const struct pw_registry_events registry_events = {
        .version = PW_VERSION_REGISTRY_EVENTS,
        .global = on_registry_global,
        .global_remove = on_registry_global_remove,
    };

    // Add listener
    pw_registry_add_listener(s_data.registry, &s_data.registry_listener,
                            &registry_events, nullptr);
}

void QPipeWireThread::reconnect(uint32_t nodeId)
{
    // Check if this node is a sink (needs monitoring)
    bool is_sink = s_isSinkMap.value(nodeId, false);

    // Disconnect current stream if active
    if (s_data.stream) {
        pw_stream_disconnect(s_data.stream);
        pw_stream_destroy(s_data.stream);
        s_data.stream = nullptr;
    }

    // Create stream properties
    pw_properties *props = pw_properties_new(
        PW_KEY_MEDIA_TYPE, "Audio",
        PW_KEY_MEDIA_CATEGORY, "Capture",
        PW_KEY_MEDIA_ROLE, "Music",
        PW_KEY_APP_NAME, "projectM",
        nullptr);

    // For sinks, set the capture.sink property to enable monitoring
    if (is_sink) {
        pw_properties_set(props, "stream.capture.sink", "true");
    }

    // Stream events
    static const struct pw_stream_events stream_events = {
        .version = PW_VERSION_STREAM_EVENTS,
        .state_changed = on_state_changed,
        .process = on_process,
    };

    // Create new stream
    s_data.stream = pw_stream_new(s_data.core, "projectM-audio-capture", props);

    if (!s_data.stream) {
        qCritical() << "Failed to create PipeWire stream";
        return;
    }

    // Add event listeners (use stream_listener, not registry_listener)
    pw_stream_add_listener(s_data.stream, &s_data.stream_listener,
                          &stream_events, &s_data);

    // Set up audio format: 32-bit float, stereo, 44.1kHz
    uint8_t buffer[1024];
    struct spa_pod_builder b = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));

    struct spa_audio_info_raw audio_info = {
        .format = SPA_AUDIO_FORMAT_F32,
        .flags = SPA_AUDIO_FLAG_NONE,
        .rate = 44100,
        .channels = 2,
    };
    audio_info.position[0] = SPA_AUDIO_CHANNEL_FL;
    audio_info.position[1] = SPA_AUDIO_CHANNEL_FR;

    const struct spa_pod *params[1];
    params[0] = spa_format_audio_raw_build(&b, SPA_PARAM_EnumFormat, &audio_info);

    // Connect to specific node
    int ret = pw_stream_connect(s_data.stream,
                               PW_DIRECTION_INPUT,
                               nodeId,  // Specific device instead of PW_ID_ANY
                               static_cast<pw_stream_flags>(
                                   PW_STREAM_FLAG_AUTOCONNECT |
                                   PW_STREAM_FLAG_MAP_BUFFERS |
                                   PW_STREAM_FLAG_RT_PROCESS),
                               params, 1);

    if (ret == 0) {
        s_currentNodeId = nodeId;
        s_currentDeviceName = s_sourceList.value(nodeId);
        emit deviceChanged();
    } else {
        qCritical() << "Failed to connect to node" << nodeId << ":" << strerror(-ret);
    }
}

int QPipeWireThread::reconnect_callback(struct spa_loop *loop, bool async, uint32_t seq,
                                         const void *data, size_t size, void *user_data)
{
    Q_UNUSED(loop);
    Q_UNUSED(async);
    Q_UNUSED(seq);
    Q_UNUSED(size);

    QPipeWireThread *self = static_cast<QPipeWireThread *>(user_data);
    uint32_t nodeId;
    memcpy(&nodeId, data, sizeof(nodeId));
    self->reconnect(nodeId);
    return 0;
}

void QPipeWireThread::connectDevice(const QModelIndex& index)
{
    if (!index.isValid()) return;

    // Get node ID from model data (DisplayRole gives the name, we need the ID)
    // The model stores sorted keys, use the row to look up from our sorted list
    QList<uint32_t> ids = s_sourceList.keys();
    std::sort(ids.begin(), ids.end());
    if (index.row() < ids.size()) {
        connectDeviceById(ids[index.row()]);
    }
}

void QPipeWireThread::connectDeviceById(uint32_t nodeId)
{
    if (!s_data.loop) return;

    // Dispatch reconnect to PipeWire thread via loop invoke
    pw_loop_invoke(pw_main_loop_get_loop(s_data.loop),
                   reconnect_callback, 0, &nodeId, sizeof(nodeId),
                   false, this);
}

void QPipeWireThread::readSettings()
{
    QSettings settings("projectM", "qprojectM-pipewire");

    bool tryFirstMonitor = settings.value("tryFirstAvailableMonitor", true).toBool();
    uint32_t preferredNodeId = settings.value("pipeWireNodeId", 0).toUInt();

    // Give time for device enumeration to complete
    QThread::msleep(100);

    if (tryFirstMonitor) {
        // Find first monitor source (system audio)
        for (auto it = s_sourceList.begin(); it != s_sourceList.end(); ++it) {
            if (it.value().contains("[Monitor]")) {
                reconnect(it.key());
                return;
            }
        }
    } else if (preferredNodeId != 0 && s_sourceList.contains(preferredNodeId)) {
        reconnect(preferredNodeId);
        return;
    }

    // Fallback: use first available source
    if (!s_sourceList.isEmpty()) {
        reconnect(s_sourceList.keys().first());
    }
}

void QPipeWireThread::cleanup()
{
    // Signal the PipeWire main loop to stop (thread-safe)
    if (s_data.loop) {
        pw_main_loop_quit(s_data.loop);
    }

    // Wait for the thread to finish before destroying resources
    wait();

    if (s_data.stream) {
        pw_stream_destroy(s_data.stream);
        s_data.stream = nullptr;
    }

    if (s_data.registry) {
        pw_proxy_destroy(reinterpret_cast<struct pw_proxy*>(s_data.registry));
        s_data.registry = nullptr;
    }

    if (s_data.core) {
        pw_core_disconnect(s_data.core);
        s_data.core = nullptr;
    }

    if (s_context) {
        pw_context_destroy(s_context);
        s_context = nullptr;
    }

    if (s_data.loop) {
        pw_main_loop_destroy(s_data.loop);
        s_data.loop = nullptr;
    }

    // Do not call pw_deinit() here — PipeWire does not support
    // re-initialization after deinit within the same process.
}

static bool s_pwInitialized = false;

void QPipeWireThread::run()
{
    if (!s_pwInitialized) {
        pw_init(&argc, &argv);
        s_pwInitialized = true;
    }

    // Reset all static state for clean re-initialization
    memset(&s_data, 0, sizeof(s_data));
    s_sourceList.clear();
    s_isSinkMap.clear();
    s_currentNodeId = PW_ID_ANY;
    s_currentDeviceName.clear();

    s_data.loop = pw_main_loop_new(nullptr);
    s_data.mainWindow = m_qprojectM_MainWindow;
    s_data.audioMutex = s_audioMutex;

    if (!s_data.loop) {
        qCritical() << "Failed to create PipeWire main loop";
        emit threadCleanedUp();
        return;
    }

    // Create context and core for device enumeration (context stored for cleanup)
    s_context = pw_context_new(pw_main_loop_get_loop(s_data.loop), nullptr, 0);
    struct pw_context *context = s_context;
    if (!context) {
        qCritical() << "Failed to create PipeWire context";
        pw_main_loop_destroy(s_data.loop);
        emit threadCleanedUp();
        return;
    }

    s_data.core = pw_context_connect(context, nullptr, 0);
    if (!s_data.core) {
        qCritical() << "Failed to connect to PipeWire daemon";
        pw_context_destroy(context);
        pw_main_loop_destroy(s_data.loop);
        emit threadCleanedUp();
        return;
    }

    // Enumerate available devices
    enumerateDevices();

    // Give some time for device enumeration to complete
    // Process events for a bit to populate the device list
    for (int i = 0; i < 10; ++i) {
        pw_loop_iterate(pw_main_loop_get_loop(s_data.loop), 10);
    }

    // Read settings and connect to appropriate device
    readSettings();

    // Run the main loop
    pw_main_loop_run(s_data.loop);

    emit threadCleanedUp();
}
