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

#include "QAudioDeviceChooser.hpp"
#include "QAudioDeviceModel.hpp"
#include "QAudioBackend.hpp"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QListView>
#include <QSettings>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QtDebug>

QAudioDeviceChooser::QAudioDeviceChooser(QAudioBackend *backend, QWidget *parent)
    : QDialog(parent),
      m_backend(backend),
      m_deviceModel(new QAudioDeviceModel(backend, this)),
      m_devicesListView(nullptr),
      m_autoDetectCheckBox(nullptr),
      m_infoLabel(nullptr)
{
    buildUi();
    readSettings();
}

void QAudioDeviceChooser::buildUi()
{
    setWindowTitle(tr("Audio Device Settings"));
    resize(380, 271);

    QVBoxLayout *mainVBox = new QVBoxLayout();

    // Info / instruction label
    m_infoLabel = new QLabel(tr("Select a source device below."), this);
    mainVBox->addWidget(m_infoLabel);

    // Device list view
    m_devicesListView = new QListView(this);
    m_devicesListView->setAutoFillBackground(true);
    m_devicesListView->setToolTip(tr("Double click a source device to activate it."));
    m_devicesListView->setModel(m_deviceModel);
    mainVBox->addWidget(m_devicesListView);

    // Auto-detect checkbox
    m_autoDetectCheckBox = new QCheckBox(tr("Auto-detect best source"), this);
    m_autoDetectCheckBox->setToolTip(
        tr("Automatically select the best available audio source on startup. "
           "This is the recommended way to get projectM to visualize sound "
           "without specifying a device explicitly."));
    mainVBox->addWidget(m_autoDetectCheckBox);

    // If the backend does not support device switching, disable the list
    if (!m_backend->supportsDeviceSwitching()) {
        m_devicesListView->setEnabled(false);
        m_autoDetectCheckBox->setEnabled(false);
        m_infoLabel->setText(
            tr("This audio backend does not support device switching."));
    }

    // Horizontal layout: main content + button box
    QHBoxLayout *hBox = new QHBoxLayout(this);
    hBox->addLayout(mainVBox);

    QDialogButtonBox *buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok, Qt::Vertical, this);
    hBox->addWidget(buttonBox);

    setLayout(hBox);

    // Connections
    connect(buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

    connect(m_autoDetectCheckBox, SIGNAL(stateChanged(int)),
            this, SLOT(onAutoDetectToggled(int)));

    connect(m_devicesListView, SIGNAL(doubleClicked(const QModelIndex&)),
            this, SLOT(onDeviceDoubleClicked(const QModelIndex&)));
}

void QAudioDeviceChooser::onAutoDetectToggled(int state)
{
    m_devicesListView->setEnabled(state != Qt::Checked);
}

void QAudioDeviceChooser::onDeviceDoubleClicked(const QModelIndex &index)
{
    if (!index.isValid())
        return;

    QString deviceId = index.data(QAudioDeviceModel::DeviceIdRole).toString();
    if (!deviceId.isEmpty()) {
        m_backend->selectDevice(deviceId);
    }
}

void QAudioDeviceChooser::writeSettings()
{
    QSettings settings("projectM", "qprojectM");
    settings.setValue("audioDeviceChooser/autoDetect",
                      m_autoDetectCheckBox->checkState() == Qt::Checked);

    QString currentId = m_backend->currentDeviceId();
    if (!currentId.isEmpty()) {
        settings.setValue("audioDeviceChooser/deviceId", currentId);
    }
}

void QAudioDeviceChooser::readSettings()
{
    QSettings settings("projectM", "qprojectM");

    bool autoDetect = settings.value("audioDeviceChooser/autoDetect", true).toBool();
    m_autoDetectCheckBox->setCheckState(autoDetect ? Qt::Checked : Qt::Unchecked);

    if (autoDetect) {
        m_devicesListView->setEnabled(false);
    } else {
        m_devicesListView->setEnabled(true);
    }
}

void QAudioDeviceChooser::open()
{
    m_deviceModel->refresh();
    show();
}
