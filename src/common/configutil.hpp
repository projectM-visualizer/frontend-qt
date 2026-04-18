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

#ifndef CONFIGUTIL_HPP
#define CONFIGUTIL_HPP

#include <QString>

/**
 * Locates and returns the path to the projectM configuration file.
 *
 * Search order:
 *   1. ~/.projectM/config.inp
 *   2. $XDG_CONFIG_HOME/projectM/config.inp  (defaults to ~/.config)
 *   3. Copies the installed default config into the XDG location
 *   4. Falls back to the installed default config
 *   5. Returns an empty QString if nothing is found (projectM uses built-in defaults)
 */
QString readProjectMConfig(const QString &installPrefix);

#endif // CONFIGUTIL_HPP
