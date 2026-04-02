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
#include "QPipeWireDeviceChooser.hpp"
#include <qprojectm_mainwindow.hpp>

#include <projectM-4/projectM.h>

#include <QApplication>
#include <QAction>
#include <QDir>
#include <QFile>
#include <QFileInfo>

#define CONFIG_FILE "/share/projectM/config.inp"

QString read_config();

class ProjectMApplication : public QApplication {
public:
    ProjectMApplication(int& argc, char ** argv) :
        QApplication(argc, argv) {
    }
    virtual ~ProjectMApplication() { }

    // catch exceptions which are thrown in slots
    virtual bool notify(QObject * receiver, QEvent * event) {
        try {
            return QApplication::notify(receiver, event);
        } catch (std::exception& e) {
            qCritical() << "Exception thrown:" << e.what();
        }
        return false;
    }
};

int main(int argc, char*argv[])
{
    // projectM 4.x: Set default OpenGL surface format before creating QApplication
    QSurfaceFormat format;
    format.setRenderableType(QSurfaceFormat::OpenGL); // Force desktop GL (not GLES) on Wayland/EGL
    format.setVersion(3, 3);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setDepthBufferSize(24);
    format.setStencilBufferSize(8);
    format.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
    format.setSwapInterval(1); // Enable vsync
    QSurfaceFormat::setDefaultFormat(format);

    ProjectMApplication app(argc, argv);

    QString config_file;
    config_file = read_config();

    QMutex audioMutex;

    QProjectM_MainWindow * mainWindow = new QProjectM_MainWindow(config_file, &audioMutex);

    // Create PipeWire audio settings action
    QAction pipeWireAction("PipeWire audio settings...", mainWindow);
    mainWindow->registerSettingsAction(&pipeWireAction);

    mainWindow->setAttribute(Qt::WA_ShowWithoutActivating, false);  // Ensure window activates
    mainWindow->setWindowState(Qt::WindowNoState);  // Not minimized/maximized
    mainWindow->show();  // Show window
    mainWindow->raise();  // Bring window to front
    mainWindow->activateWindow();  // Make it the active window

    // Process events to ensure window is mapped
    app.processEvents();

    // Set the audio mutex for thread synchronization BEFORE starting the thread
    QPipeWireThread::setAudioMutex(&audioMutex);

    QPipeWireThread * pipewireThread = new QPipeWireThread(argc, argv, mainWindow);
    pipewireThread->start();

    // Create device chooser dialog
    QPipeWireDeviceChooser devChooser(pipewireThread, mainWindow);

    // Connect menu action to dialog
    QApplication::connect(&pipeWireAction, SIGNAL(triggered()),
                         &devChooser, SLOT(open()));

    int ret = app.exec();

    if (pipewireThread != nullptr) {
        pipewireThread->writeSettings();
        pipewireThread->cleanup();
        delete pipewireThread;
    }

    devChooser.writeSettings();

    return ret;
}

QString read_config()
{
    // Build default config path from install prefix
    QString defaultConfig = QString(PROJECTM_PREFIX) + CONFIG_FILE;

    const char* home = getenv("HOME");
    const char* xdg_home = getenv("XDG_CONFIG_HOME");

    if (!home) {
        return defaultConfig;
    }

    // Try ~/.projectM/config.inp
    QString userConfig = QString(home) + "/.projectM/config.inp";
    if (QFileInfo::exists(userConfig)) {
        return userConfig;
    }

    // Try $XDG_CONFIG_HOME/projectM/config.inp (defaults to ~/.config per XDG spec)
    QString xdgConfigHome = xdg_home ? QString(xdg_home) : QString(home) + "/.config";
    {
        QString xdgConfig = xdgConfigHome + "/projectM/config.inp";
        if (QFileInfo::exists(xdgConfig)) {
            return xdgConfig;
        }
    }

    // Try to create user config by copying default
    QString configDir = xdgConfigHome + "/projectM";

    QDir().mkpath(configDir);
    QString newConfig = configDir + "/config.inp";

    if (QFile::exists(defaultConfig)) {
        if (QFile::copy(defaultConfig, newConfig)) {
            return newConfig;
        }
    }

    // Fall back to default config
    if (QFile::exists(defaultConfig)) {
        return defaultConfig;
    }

    // No config found — projectM will use built-in defaults
    qWarning() << "No projectM config file found, using built-in defaults";
    return QString();
}
