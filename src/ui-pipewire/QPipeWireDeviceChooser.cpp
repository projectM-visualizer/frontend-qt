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

#include "QPipeWireDeviceChooser.hpp"
#include <QSettings>
#include <QtDebug>
#include <QHBoxLayout>

void QPipeWireDeviceChooser::writeSettings()
{
    QSettings settings("projectM", "qprojectM-pipewire");
    settings.setValue("tryFirstAvailableMonitor",
                      this->tryFirstAvailableMonitorCheckBox->checkState() == Qt::Checked);

    if (m_pipeWireThread != nullptr) {
        settings.setValue("pipeWireNodeId", QPipeWireThread::currentNodeId());
    }
}

void QPipeWireDeviceChooser::readSettings()
{
    QSettings settings("projectM", "qprojectM-pipewire");

    bool tryFirst = settings.value("tryFirstAvailableMonitor", true).toBool();

    this->tryFirstAvailableMonitorCheckBox->setCheckState(
        tryFirst ? Qt::Checked : Qt::Unchecked);

    if (tryFirst) {
        this->devicesListView->setEnabled(false);
    } else {
        this->devicesListView->setEnabled(true);
    }
}

void QPipeWireDeviceChooser::updateDevicesListViewLock(int state)
{
    devicesListView->setEnabled(state != Qt::Checked);

    if (state == Qt::Checked) {
        if (m_pipeWireThread != nullptr) {
            // Find and connect to first monitor source
            const QHash<uint32_t, QString>& sources = QPipeWireThread::sourceList();
            for (auto it = sources.begin(); it != sources.end(); ++it) {
                if (it.value().contains("[Monitor]")) {
                    m_pipeWireThread->connectDeviceById(it.key());
                    break;
                }
            }
        }
    }
}

QPipeWireDeviceChooser::QPipeWireDeviceChooser(QPipeWireThread * pipeWireThread,
                                               QWidget * parent,
                                               Qt::WindowFlags f)
    : QDialog(parent, f),
      m_pipeWireDeviceModel(QPipeWireThread::sourceList(),
                            QPipeWireThread::currentNodeId(),
                            this),
      m_pipeWireThread(pipeWireThread)
{
    setupUi(this);
    readSettings();
    this->devicesListView->setModel(&m_pipeWireDeviceModel);

    QHBoxLayout * hboxLayout = new QHBoxLayout();
    hboxLayout->addWidget(this->layoutWidget);
    this->setLayout(hboxLayout);

    connect(tryFirstAvailableMonitorCheckBox,
            SIGNAL(stateChanged(int)), this, SLOT(updateDevicesListViewLock(int)));

    // Double-click to connect to device
    connect(devicesListView, SIGNAL(doubleClicked(const QModelIndex&)),
            m_pipeWireThread, SLOT(connectDevice(const QModelIndex&)));

    connect(m_pipeWireThread, SIGNAL(deviceChanged()),
            &m_pipeWireDeviceModel, SLOT(updateItemHighlights()));
}

void QPipeWireDeviceChooser::open()
{
    this->show();
}
