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
    // Prefer PipeWire > PulseAudio > JACK based on what was compiled in.
    // Actual server availability is checked when the backend starts.
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

int main(int argc, char *argv[])
{
    // Set default OpenGL surface format before creating QApplication
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

    // 1. Parse --backend from command line
    QString requestedBackend;
    for (int i = 1; i < argc; ++i) {
        if (QString(argv[i]) == "--backend" && i + 1 < argc) {
            requestedBackend = QString(argv[i + 1]).toLower();
            break;
        }
    }

    // 2. Check QSettings
    if (requestedBackend.isEmpty()) {
        QSettings settings("projectM", "qprojectM");
        requestedBackend = settings.value("audioBackend").toString().toLower();
    }

    // 3. Auto-detect
    if (requestedBackend.isEmpty()) {
        requestedBackend = autoDetectBackend();
    }

    if (requestedBackend.isEmpty()) {
        qCritical() << "No audio backend available.";
        return 1;
    }

    QAudioBackend *backend = createBackend(requestedBackend);
    if (!backend) {
        qCritical() << "Unknown or disabled audio backend:" << requestedBackend;
        return 1;
    }

    qDebug() << "Using audio backend:" << backend->backendName();

    // Save the chosen backend for next launch
    {
        QSettings settings("projectM", "qprojectM");
        settings.setValue("audioBackend", requestedBackend);
    }

    QString config_file = readProjectMConfig(PROJECTM_PREFIX);
    QMutex audioMutex;

    QProjectM_MainWindow *mainWindow = new QProjectM_MainWindow(config_file, &audioMutex);

    QAction audioAction(backend->backendName() + " audio settings...", mainWindow);
    mainWindow->registerSettingsAction(&audioAction);

    mainWindow->setAttribute(Qt::WA_ShowWithoutActivating, false);
    mainWindow->setWindowState(Qt::WindowNoState);
    mainWindow->show();
    mainWindow->raise();
    mainWindow->activateWindow();
    app.processEvents();

    backend->start(mainWindow, &audioMutex);

    QAudioDeviceChooser devChooser(backend, mainWindow);
    QObject::connect(&audioAction, SIGNAL(triggered()), &devChooser, SLOT(open()));

    int ret = app.exec();

    mainWindow->unregisterSettingsAction(&audioAction);
    devChooser.writeSettings();
    backend->writeSettings();
    backend->stop();
    delete backend;

    return ret;
}
