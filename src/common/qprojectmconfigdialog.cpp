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
#include "qprojectmconfigdialog.hpp"
#include <QtDebug>
#include <QAction>
#include <QMutexLocker>
#include "qplaylistfiledialog.hpp"
#include <QSettings>
#include "qprojectmwidget.hpp"
#include "configfile.hpp"
#include <projectM-4/parameters.h>

QProjectMConfigDialog::QProjectMConfigDialog(const QString& configFile, QProjectMWidget * qprojectMWidget, QWidget * parent, Qt::WindowFlags f) : QDialog(parent, f), _settings("projectM", "qprojectM"), _configFile(configFile), _qprojectMWidget(qprojectMWidget) {


	_ui.setupUi(this);

	QHBoxLayout * hboxLayout = new QHBoxLayout();

	hboxLayout->addWidget(_ui.layoutWidget);
	this->setLayout(hboxLayout);

	connect(_ui.buttonBox, SIGNAL(clicked(QAbstractButton*)), this, SLOT(buttonBoxHandler(QAbstractButton*)));
	connect(this, SIGNAL(projectM_Reset()), _qprojectMWidget, SLOT(resetProjectM()));
	connect (_ui.startupPlaylistFileToolButton, SIGNAL(clicked()), this, SLOT(openPlaylistFileDialog()));
	connect (_ui.startupPlaylistDirectoryToolButton, SIGNAL(clicked()), this, SLOT(openPlaylistDirectoryDialog()));
	connect (_ui.titleFontPathToolButton, SIGNAL(clicked()), this, SLOT(openTitleFontFileDialog()));
	connect (_ui.menuFontPathToolButton, SIGNAL(clicked()), this, SLOT(openMenuFontFileDialog()));
	loadConfig();
}

void QProjectMConfigDialog::buttonBoxHandler(QAbstractButton * button) {

	switch (_ui.buttonBox->standardButton(button)) {
		case QDialogButtonBox::Close:
			this->hide();
			break;
		case QDialogButtonBox::Save:
			saveConfig();
			applyLiveSettings();
			break;
		case QDialogButtonBox::Reset:
			loadConfig();
			break;
		default:
			break;
	}
}

void QProjectMConfigDialog::openPlaylistFileDialog() {

	QPlaylistFileDialog dialog(this);

	dialog.setAllowFileSelect(true);
	dialog.setAllowDirectorySelect(false);

	if (dialog.exec())
	{
		Q_ASSERT(!dialog.selectedFiles().empty());
		_ui.startupPlaylistFileLineEdit->setText(dialog.selectedFiles()[0]);

	}
}


void QProjectMConfigDialog::openPlaylistDirectoryDialog() {

	QPlaylistFileDialog dialog(this);

	dialog.setAllowFileSelect(false);
	dialog.setAllowDirectorySelect(true);

	if (dialog.exec())
	{
		Q_ASSERT(!dialog.selectedFiles().empty());
		_ui.startupPlaylistDirectoryLineEdit->setText(dialog.selectedFiles()[0]);

	}
}
void QProjectMConfigDialog::openMenuFontFileDialog() {


	QFileDialog dialog(this, "Select a menu font", _settings.value("Menu Font Directory", QString()).toString(), "True Type Fonts (*.ttf)" );
	dialog.setFileMode(QFileDialog::ExistingFile);

	if (dialog.exec()) {
		Q_ASSERT(!dialog.selectedFiles().empty());
		_ui.menuFontPathLineEdit->setText(dialog.selectedFiles()[0]);

		_settings.setValue("Menu Font Directory", dialog.directory().absolutePath());
	}


}

void QProjectMConfigDialog::openTitleFontFileDialog() {
	QFileDialog dialog(this, "Select a title font", _settings.value("Title Font Directory", QString()).toString(), "True Type Fonts (*.ttf)" );
	dialog.setFileMode(QFileDialog::ExistingFile);

	if (dialog.exec()) {
		Q_ASSERT(!dialog.selectedFiles().empty());
		_ui.titleFontPathLineEdit->setText(dialog.selectedFiles()[0]);
		_settings.setValue("Title Font Directory", dialog.directory().absolutePath());
	}

}

void QProjectMConfigDialog::saveConfig() {
	// projectM 4.x: Use ConfigFile directly instead of removed settings API
	try {
		ConfigFile config(_configFile.toStdString());

		config.add("Mesh X", _ui.meshSizeWidthSpinBox->value());
		config.add("Mesh Y", _ui.meshSizeHeightSpinBox->value());
		config.add("Window Height", _ui.windowHeightSpinBox->value());
		config.add("Window Width", _ui.windowWidthSpinBox->value());
		config.add("Preset Path", _ui.startupPlaylistDirectoryLineEdit->text().toStdString());
		config.add("Texture Size", _ui.textureSizeComboBox->itemData(_ui.textureSizeComboBox->currentIndex()).toInt());
		config.add("Soft Cut Duration", _ui.smoothPresetDurationSpinBox->value());
		config.add("Preset Duration", _ui.presetDurationSpinBox->value());
		config.add("FPS", _ui.maxFPSSpinBox->value());
		config.add("Aspect Correction", _ui.useAspectCorrectionCheckBox->checkState() == Qt::Checked);
		config.add("Beat Sensitivity", _ui.beatSensitivitySpinBox->value());
		config.add("Easter Egg", _ui.easterEggParameterSpinBox->value());
		config.add("Shuffle Enabled", _ui.shuffleOnStartupCheckBox->checkState() == Qt::Checked);
		config.add("Soft Cut Ratings Enabled", _ui.softCutRatingsEnabledCheckBox->checkState() == Qt::Checked);

		// Write config to file
		std::ofstream ofs(_configFile.toStdString().c_str());
		ofs << config;
	} catch (ConfigFile::file_not_found&) {
		qWarning() << "Could not write config file:" << _configFile;
	}

	QSettings qSettings("projectM", "qprojectM");
	qSettings.setValue("FullscreenOnStartup", _ui.fullscreenOnStartupCheckBox->checkState() == Qt::Checked);
	qSettings.setValue("MenuOnStartup", _ui.menuOnStartupCheckBox->checkState() == Qt::Checked);
	qSettings.setValue("PlaylistFile", _ui.startupPlaylistFileLineEdit->text());
	qSettings.setValue("MouseHideOnTimeout", _ui.mouseHideTimeoutSpinBox->value());
}

void QProjectMConfigDialog::applyLiveSettings() {
	// Push setting changes to the running projectM instance so they take
	// effect without an app restart. Only values that can be safely changed
	// at runtime via the public C API are applied here. Values that need
	// GL context recreation (texture size) or playlist reload (preset path,
	// shuffle) still require a restart.
	if (!_qprojectMWidget || !_qprojectMWidget->qprojectM()) {
		return;
	}
	auto *pm = _qprojectMWidget->qprojectM()->instance();
	if (!pm) {
		return;
	}

	// Resize the top-level window first. Qt's resize event flows down to
	// QProjectMWidget::resizeGL which already handles devicePixelRatio
	// scaling and queues projectm_set_window_size on the render thread.
	// Doing this directly would skip DPR scaling and desync projectM's
	// internal size from the actual widget framebuffer.
	if (auto *topWindow = _qprojectMWidget->window()) {
		topWindow->resize(_ui.windowWidthSpinBox->value(),
		                  _ui.windowHeightSpinBox->value());
	}

	// All other projectM API calls share the render thread's mutex to
	// avoid races with paintGL. The mutex is recursive-safe via QMutex.
	QMutexLocker projectMLock(_qprojectMWidget->projectMMutex());

	projectm_set_fps(pm, _ui.maxFPSSpinBox->value());
	projectm_set_aspect_correction(pm, _ui.useAspectCorrectionCheckBox->checkState() == Qt::Checked);
	projectm_set_beat_sensitivity(pm, static_cast<float>(_ui.beatSensitivitySpinBox->value()));
	projectm_set_soft_cut_duration(pm, _ui.smoothPresetDurationSpinBox->value());
	projectm_set_preset_duration(pm, _ui.presetDurationSpinBox->value());
	projectm_set_easter_egg(pm, static_cast<float>(_ui.easterEggParameterSpinBox->value()));
	projectm_set_mesh_size(pm,
	                       static_cast<size_t>(_ui.meshSizeWidthSpinBox->value()),
	                       static_cast<size_t>(_ui.meshSizeHeightSpinBox->value()));
}



void QProjectMConfigDialog::populateTextureSizeComboBox() {

	_ui.textureSizeComboBox->clear();
	for (int textureSize = 1<<1; textureSize < 1<<14; textureSize<<=1) {
		_ui.textureSizeComboBox->addItem(QString("%1").arg(textureSize), textureSize);
	}
}

void QProjectMConfigDialog::loadConfig() {
	// projectM 4.x: Use ConfigFile directly instead of removed settings API
	try {
		ConfigFile config(_configFile.toStdString());

		_ui.meshSizeWidthSpinBox->setValue(config.read("Mesh X", 32));
		_ui.meshSizeHeightSpinBox->setValue(config.read("Mesh Y", 24));
		_ui.startupPlaylistDirectoryLineEdit->setText(QString::fromStdString(config.read<std::string>("Preset Path", "")));
		_ui.useAspectCorrectionCheckBox->setCheckState(config.read("Aspect Correction", true) ? Qt::Checked : Qt::Unchecked);
		_ui.maxFPSSpinBox->setValue(config.read("FPS", 60));
		_ui.beatSensitivitySpinBox->setValue(config.read("Beat Sensitivity", 1.0));
		_ui.windowHeightSpinBox->setValue(config.read("Window Height", 768));
		_ui.windowWidthSpinBox->setValue(config.read("Window Width", 1024));
		_ui.shuffleOnStartupCheckBox->setCheckState(config.read("Shuffle Enabled", false) ? Qt::Checked : Qt::Unchecked);

		populateTextureSizeComboBox();
		int textureSize = config.read("Texture Size", 1024);
		_ui.textureSizeComboBox->insertItem(0, QString("%1").arg(textureSize), textureSize);
		_ui.textureSizeComboBox->setCurrentIndex(0);

		_ui.smoothPresetDurationSpinBox->setValue(config.read("Soft Cut Duration", 3));
		_ui.presetDurationSpinBox->setValue(config.read("Preset Duration", 30));
		_ui.easterEggParameterSpinBox->setValue(config.read("Easter Egg", 0.0));
		_ui.softCutRatingsEnabledCheckBox->setCheckState(config.read("Soft Cut Ratings Enabled", false) ? Qt::Checked : Qt::Unchecked);
	} catch (ConfigFile::file_not_found&) {
		qWarning() << "Could not read config file:" << _configFile << ", using defaults";
		// Set default values
		_ui.meshSizeWidthSpinBox->setValue(32);
		_ui.meshSizeHeightSpinBox->setValue(24);
		_ui.windowHeightSpinBox->setValue(768);
		_ui.windowWidthSpinBox->setValue(1024);
		_ui.maxFPSSpinBox->setValue(60);
		_ui.beatSensitivitySpinBox->setValue(1.0);
		populateTextureSizeComboBox();
	}

	QSettings qSettings("projectM", "qprojectM");
	_ui.fullscreenOnStartupCheckBox->setCheckState(qSettings.value("FullscreenOnStartup", false).toBool() ? Qt::Checked : Qt::Unchecked);
	_ui.menuOnStartupCheckBox->setCheckState(qSettings.value("MenuOnStartup", false).toBool() ? Qt::Checked : Qt::Unchecked);
	_ui.startupPlaylistFileLineEdit->setText(qSettings.value("PlaylistFile", QString()).toString());
	_ui.mouseHideTimeoutSpinBox->setValue(qSettings.value("MouseHideOnTimeout", 5).toInt());
}
