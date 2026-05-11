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

#include "configutil.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QtDebug>

static const char* CONFIG_FILE = "/share/projectM/config.inp";

QString readProjectMConfig(const QString &installPrefix)
{
    QString defaultConfig = installPrefix + CONFIG_FILE;

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

    // No config found -- projectM will use built-in defaults
    qWarning() << "No projectM config file found, using built-in defaults";
    return QString();
}
