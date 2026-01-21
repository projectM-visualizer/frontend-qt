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

#ifndef QPIPEWIRE_AUDIO_THREAD
#define QPIPEWIRE_AUDIO_THREAD

#include <QObject>
#include <QTimer>
#include <QThread>
#include <QString>
#include <QMutex>
#include <QtDebug>

#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>

#include "qprojectm_mainwindow.hpp"
#include <QHash>
#include <QModelIndex>

class QPipeWireThread : public QThread
{
    Q_OBJECT

public:
    QPipeWireThread() {}
    QPipeWireThread(int _argc, char **_argv, QProjectM_MainWindow * qprojectM_MainWindow);
    virtual ~QPipeWireThread();
    void run() override;

    QMutex * mutex();
    void writeSettings();
    void readSettings();

    // Device enumeration
    static const QHash<uint32_t, QString>& sourceList() { return s_sourceList; }
    static QString currentDeviceName() { return s_currentDeviceName; }
    static uint32_t currentNodeId() { return s_currentNodeId; }

public slots:
    void cleanup();
    void connectDevice(const QModelIndex& index);
    void connectDeviceById(uint32_t nodeId);

signals:
    void threadCleanedUp();
    void deviceChanged();

private:
    struct AudioData {
        pw_main_loop *loop;
        pw_stream *stream;
        QProjectM_MainWindow *mainWindow;
        QMutex *audioMutex;
        pw_core *core;
        pw_registry *registry;
        struct spa_hook registry_listener;
        struct spa_hook stream_listener;
    };

    static void on_process(void *userdata);
    static void on_state_changed(void *data, enum pw_stream_state old_state,
                                  enum pw_stream_state state, const char *error);

    // Registry callbacks for device enumeration
    static void on_registry_global(void *data, uint32_t id,
                                   uint32_t permissions,
                                   const char *type,
                                   uint32_t version,
                                   const struct spa_dict *props);
    static void on_registry_global_remove(void *data, uint32_t id);

    void enumerateDevices();
    void reconnect(uint32_t nodeId);

    int argc;
    char **argv;
    QProjectM_MainWindow *m_qprojectM_MainWindow;
    static QMutex *s_audioMutex;
    static AudioData s_data;

    // Device storage
    static QHash<uint32_t, QString> s_sourceList;  // node_id -> display_name
    static QString s_currentDeviceName;
    static uint32_t s_currentNodeId;
};

#endif
