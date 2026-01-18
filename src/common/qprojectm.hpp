/**
 * projectM-qt -- Qt4 based projectM GUI
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

#ifndef QPROJECTM_HPP
#define QPROJECTM_HPP

#include <projectM-4/projectM.h>

#include <QObject>
#include <QString>

class QProjectM : public QObject
{

Q_OBJECT

public:
    explicit QProjectM(const QString& config_file)
        : _projectM(projectm_create())
    {
        // projectM 4.x: projectm_create() takes no arguments
        // TODO: Load settings from config_file if needed

        // projectM 4.x: Callback signatures changed
        projectm_set_preset_switch_requested_event_callback(_projectM, &QProjectM::presetSwitchRequestedEvent, this);
        projectm_set_preset_switch_failed_event_callback(_projectM, &QProjectM::presetSwitchFailedEvent, this);

        // projectM 4.x: Rating callback removed
    }

    projectm* instance() const
    {
        return _projectM;
    }

signals:

    // projectM 4.x: Switch requested callback has no index
    void presetSwitchRequestedSignal(bool is_hard_cut) const;

    // projectM 4.x: Failed callback now has filename instead of index
    void presetSwitchFailedSignal(const QString& filename, const QString& message) const;

protected:

    // projectM 4.x: New callback signature
    static void presetSwitchRequestedEvent(bool is_hard_cut, void* context)
    {
        auto qProjectM = reinterpret_cast<QProjectM*>(context);
        qProjectM->presetSwitchRequestedSignal(is_hard_cut);
    }

    // projectM 4.x: New callback signature
    static void presetSwitchFailedEvent(const char* preset_filename, const char* message, void* context)
    {
        auto qProjectM = reinterpret_cast<QProjectM*>(context);
        qProjectM->presetSwitchFailedSignal(QString(preset_filename), QString(message));
    }

    projectm* _projectM{ nullptr };
};

#endif
