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

#include "QAudioBackend.hpp"
#include "QAudioDeviceChooser.hpp"
#include "configutil.hpp"
#include <qprojectm_mainwindow.hpp>

#include <QApplication>
#include <QAction>
#include <QActionGroup>
#include <QMenu>
#include <QMenuBar>
#include <QMutex>
#include <QSettings>
#include <QSurfaceFormat>
#include <QtDebug>

#include <exception>

#ifdef ENABLE_PIPEWIRE
#include "QPipeWireBackend.hpp"
#endif

#ifdef ENABLE_PULSEAUDIO
#include "QPulseAudioBackend.hpp"
#endif

#ifdef ENABLE_JACK
#include "QJackBackend.hpp"
#endif

class ProjectMApplication : public QApplication {
public:
    ProjectMApplication(int& argc, char ** argv) :
        QApplication(argc, argv) {
    }
    virtual ~ProjectMApplication() { }

    virtual bool notify(QObject * receiver, QEvent * event) {
        try {
            return QApplication::notify(receiver, event);
        } catch (std::exception& e) {
            qCritical() << "Exception thrown:" << e.what();
        }
        return false;
    }
};

static QString autoDetectBackend()
{
#ifdef ENABLE_PIPEWIRE
    return "pipewire";
#elif defined(ENABLE_PULSEAUDIO)
    return "pulseaudio";
#elif defined(ENABLE_JACK)
    return "jack";
#else
    return QString();
#endif
}

static QAudioBackend* createBackend(const QString &name)
{
#ifdef ENABLE_PIPEWIRE
    if (name == "pipewire") return new QPipeWireBackend();
#endif
#ifdef ENABLE_PULSEAUDIO
    if (name == "pulseaudio") return new QPulseAudioBackend();
#endif
#ifdef ENABLE_JACK
    if (name == "jack") return new QJackBackend();
#endif
    return nullptr;
}

struct BackendEntry {
    QString id;
    QString label;
};

static QList<BackendEntry> availableBackends()
{
    QList<BackendEntry> list;
    BackendEntry e;
#ifdef ENABLE_PIPEWIRE
    e.id = "pipewire"; e.label = "PipeWire"; list.append(e);
#endif
#ifdef ENABLE_PULSEAUDIO
    e.id = "pulseaudio"; e.label = "PulseAudio"; list.append(e);
#endif
#ifdef ENABLE_JACK
    e.id = "jack"; e.label = "JACK"; list.append(e);
#endif
    return list;
}

int main(int argc, char *argv[])
{
    QSurfaceFormat format;
    format.setRenderableType(QSurfaceFormat::OpenGL);
    format.setVersion(3, 3);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setDepthBufferSize(24);
    format.setStencilBufferSize(8);
    format.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
    format.setSwapInterval(1);
    QSurfaceFormat::setDefaultFormat(format);

    ProjectMApplication app(argc, argv);

    // Parse --backend from command line
    QString requestedBackend;
    for (int i = 1; i < argc; ++i) {
        if (QString(argv[i]) == "--backend" && i + 1 < argc) {
            requestedBackend = QString(argv[i + 1]).toLower();
            break;
        }
    }

    if (requestedBackend.isEmpty()) {
        QSettings settings("projectM", "qprojectM");
        requestedBackend = settings.value("audioBackend").toString().toLower();
    }

    if (requestedBackend.isEmpty()) {
        requestedBackend = autoDetectBackend();
    }

    if (requestedBackend.isEmpty()) {
        qCritical() << "No audio backend available.";
        return 1;
    }

    // State
    QHash<QString, QAudioBackend*> backendCache;
    QAudioBackend *backend = nullptr;
    QAudioDeviceChooser *devChooser = nullptr;
    QAction *devAction = nullptr;
    QString activeBackendId;

    QString config_file = readProjectMConfig(PROJECTM_PREFIX);
    QMutex audioMutex;
    QProjectM_MainWindow *mainWindow = new QProjectM_MainWindow(config_file, &audioMutex);

    // -- Add backend submenu under Settings --
    QMenu *settingsMenu = mainWindow->findChild<QMenu*>("menuSettings");
    QMenu *audioMenu = new QMenu("Audio Backend", mainWindow);
    QActionGroup *backendGroup = new QActionGroup(mainWindow);
    backendGroup->setExclusive(true);

    // Device settings action
    devAction = new QAction("Audio device settings...", mainWindow);
    mainWindow->registerSettingsAction(devAction);

    QList<BackendEntry> backends = availableBackends();

    // Get or create a backend, caching instances to avoid teardown/re-init issues
    auto getOrCreateBackend = [&](const QString &id) -> QAudioBackend* {
        if (backendCache.contains(id)) {
            return backendCache.value(id);
        }
        QAudioBackend *b = createBackend(id);
        if (b) {
            backendCache.insert(id, b);
        }
        return b;
    };

    // Function to switch backends
    auto switchBackend = [&](const QString &newId) {
        if (activeBackendId == newId) {
            return;
        }

        QAudioBackend *newBackend = getOrCreateBackend(newId);
        if (!newBackend) {
            qCritical() << "Failed to create backend:" << newId;
            for (QAction *a : backendGroup->actions()) {
                if (a->data().toString() == activeBackendId) {
                    a->setChecked(true);
                }
            }
            return;
        }

        // Start the new backend (may already be started if cached and still running)
        if (!newBackend->start(mainWindow, &audioMutex)) {
            qWarning() << "Backend" << newId << "failed to start, keeping current backend";
            for (QAction *a : backendGroup->actions()) {
                if (a->data().toString() == activeBackendId) {
                    a->setChecked(true);
                }
            }
            return;
        }

        // Deactivate the old backend (keep it cached but stop audio delivery)
        if (backend) {
            backend->setActive(false);
            backend->stop();
        }

        // Tear down old device chooser
        if (devChooser) {
            devChooser->writeSettings();
            delete devChooser;
            devChooser = nullptr;
        }

        backend = newBackend;
        backend->setActive(true);
        activeBackendId = newId;

        devChooser = new QAudioDeviceChooser(backend, mainWindow);
        QObject::disconnect(devAction, nullptr, nullptr, nullptr);
        QObject::connect(devAction, SIGNAL(triggered()), devChooser, SLOT(open()));

        QSettings settings("projectM", "qprojectM");
        settings.setValue("audioBackend", newId);

        qDebug() << "Switched to audio backend:" << backend->backendName();
    };

    // Create backend selector actions
    for (const BackendEntry &entry : backends) {
        QAction *action = new QAction(entry.label, backendGroup);
        action->setCheckable(true);
        action->setData(entry.id);
        if (entry.id == requestedBackend) {
            action->setChecked(true);
        }
        QObject::connect(action, &QAction::triggered, [&switchBackend, entry]() {
            switchBackend(entry.id);
        });
        audioMenu->addAction(action);
    }

    // Add backend submenu to Settings menu
    if (settingsMenu) {
        settingsMenu->addSeparator();
        settingsMenu->addMenu(audioMenu);
    }

    mainWindow->setAttribute(Qt::WA_ShowWithoutActivating, false);
    mainWindow->setWindowState(Qt::WindowNoState);
    mainWindow->show();
    mainWindow->raise();
    mainWindow->activateWindow();
    app.processEvents();

    // Start initial backend
    switchBackend(requestedBackend);

    int ret = app.exec();

    // Cleanup
    mainWindow->unregisterSettingsAction(devAction);
    if (devChooser) {
        devChooser->writeSettings();
        delete devChooser;
    }
    // Stop and delete all cached backends
    for (QAudioBackend *b : backendCache.values()) {
        b->writeSettings();
        b->stop();
        delete b;
    }

    return ret;
}
