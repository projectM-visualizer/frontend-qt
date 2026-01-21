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

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <sys/stat.h>

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
    char num[512];
    FILE *in;
    FILE *out;

    char* home;
    char* xdg_home;

    char projectM_config[1024];
    char default_config[1024];

    strcpy(default_config, PROJECTM_PREFIX);
    strcat(default_config, CONFIG_FILE);
    printf("Default config: %s\n", default_config);

    home = getenv("HOME");
    xdg_home = getenv("XDG_CONFIG_HOME");

    strcpy(projectM_config, home);
    strcat(projectM_config, "/.projectM/config.inp");

    if ((in = fopen(projectM_config, "r")) != 0) {
        printf("reading %s \n", projectM_config);
        fclose(in);
        return projectM_config;
    }
    else {
        if (xdg_home) {
            strcpy(projectM_config, xdg_home);
            strcat(projectM_config, "/projectM/config.inp");
            if ((in = fopen(projectM_config, "r")) != 0) {
                printf("reading %s \n", projectM_config);
                fclose(in);
                return projectM_config;
            }
            else {
                strcpy(projectM_config, xdg_home);
                strcat(projectM_config, "/projectM");
            }
        }
        else {
            strcpy(projectM_config, home);
            strcat(projectM_config, "/.projectM");
        }
        mkdir(projectM_config, 0755);
        strcat(projectM_config, "/config.inp");
        printf("trying to create %s \n", projectM_config);

        if ((out = fopen(projectM_config,"w")) !=0) {

            if ((in = fopen(default_config, "r")) != 0) {

                while (fgets(num,80,in) !=NULL) {
                    fputs(num,out);
                }
                fclose(in);
                fclose(out);

                if ((in = fopen(projectM_config, "r")) != 0) {
                    printf("created %s successfully\n", projectM_config);
                    fclose(in);
                    return projectM_config;
                }
                else{printf("This shouldn't happen, using implementation defaults\n");abort();}
            }
            else{printf("Cannot find projectM default config, using implementation defaults\n");abort();}
        }
        else {
            printf("Cannot create %s, using default config file\n", projectM_config);
            if ((in = fopen(default_config, "r")) != 0) {
                printf("Successfully opened default config file\n");
                fclose(in);
                return default_config;
            }
            else{ printf("Using implementation defaults, your system is really messed up, I'm surprised we even got this far\n");  abort();}

        }

    }

    abort();
}
