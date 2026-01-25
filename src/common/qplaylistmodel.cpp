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

#include <projectM-4/projectM.h>
#include <projectM-4/playlist.h>

#include <QIcon>
#include <QXmlStreamReader>
#include <QtDebug>
#include <QFileInfo>
#include <QDir>
#include <QMessageBox>

#include "qplaylistmodel.hpp"
#include <QMimeData>

QString QPlaylistModel::PRESET_MIME_TYPE("text/x-projectM-preset");

QPlaylistModel::QPlaylistModel(projectm* _projectM, QObject * parent)
    : QAbstractTableModel(parent), m_projectM(_projectM), m_playlist(nullptr)
{
    // projectM 4.x: Create playlist manager
    m_playlist = projectm_playlist_create(m_projectM);
    if (!m_playlist) {
        qWarning() << "Failed to create playlist manager";
    }
}

QPlaylistModel::~QPlaylistModel()
{
    if (m_playlist) {
        projectm_playlist_destroy(m_playlist);
    }
}

int QPlaylistModel::rowCount(const QModelIndex & parent) const
{
    Q_UNUSED(parent);
    if (!m_playlist)
        return 0;
    return projectm_playlist_size(m_playlist);
}

int QPlaylistModel::columnCount(const QModelIndex & parent) const
{
    Q_UNUSED(parent);
    return 1;  // projectM 4.x: Only filename column
}

QVariant QPlaylistModel::data(const QModelIndex & index, int role) const
{
    if (!m_playlist || !index.isValid())
        return QVariant();

    int row = index.row();
    if (row < 0 || row >= static_cast<int>(projectm_playlist_size(m_playlist)))
        return QVariant();

    switch (role) {
        case Qt::DisplayRole:
        case URLInfoRole:
        case NameRole: {
            char* filename = projectm_playlist_item(m_playlist, row);
            if (filename) {
                QString path(filename);
                projectm_playlist_free_string(filename);

                if (role == URLInfoRole) {
                    // Return full path for URL info
                    return path;
                }
                // For DisplayRole and NameRole, return just the filename without .milk extension
                QString name = QFileInfo(path).fileName();
                if (name.endsWith(".milk", Qt::CaseInsensitive)) {
                    name.chop(5);  // Remove ".milk"
                }
                return name;
            }
            return QVariant();
        }

        case Qt::DecorationRole: {
            // Check if this is the currently playing preset
            uint32_t currentPos = projectm_playlist_get_position(m_playlist);
            if (row == static_cast<int>(currentPos)) {
                if (projectm_get_preset_locked(m_projectM)) {
                    return QIcon(":/icons/resources/icons/status_locked.png");
                }
                return QIcon(":/icons/resources/icons/status_playing.png");
            }
            return QVariant();
        }

        case Qt::ToolTipRole: {
            char* filename = projectm_playlist_item(m_playlist, row);
            if (filename) {
                QString tooltip(filename);
                projectm_playlist_free_string(filename);
                return tooltip;
            }
            return QVariant();
        }

        default:
            return QVariant();
    }
}

QVariant QPlaylistModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole)
        return QVariant();

    if (orientation == Qt::Horizontal) {
        return QString("Preset");
    }
    return QString::number(section + 1);
}

bool QPlaylistModel::setData(const QModelIndex & index, const QVariant & value, int role)
{
    Q_UNUSED(index);
    Q_UNUSED(value);
    Q_UNUSED(role);
    // projectM 4.x: Editing disabled (no ratings/names to edit)
    return false;
}

Qt::ItemFlags QPlaylistModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return Qt::ItemIsEnabled | Qt::ItemIsDropEnabled;

    return QAbstractItemModel::flags(index) | Qt::ItemIsDragEnabled;
}

void QPlaylistModel::appendRow(const QString & presetURL)
{
    if (!m_playlist)
        return;

    int oldSize = projectm_playlist_size(m_playlist);
    beginInsertRows(QModelIndex(), oldSize, oldSize);
    projectm_playlist_add_preset(m_playlist, presetURL.toLocal8Bit().data(), true);
    endInsertRows();
}

void QPlaylistModel::insertRow(int index, const QString & presetURL)
{
    if (!m_playlist)
        return;

    beginInsertRows(QModelIndex(), index, index);
    projectm_playlist_insert_preset(m_playlist, presetURL.toLocal8Bit().data(), index, true);
    endInsertRows();
}

bool QPlaylistModel::removeRow(int index, const QModelIndex & parent)
{
    Q_UNUSED(parent);
    if (!m_playlist)
        return false;

    if (index < 0 || index >= static_cast<int>(projectm_playlist_size(m_playlist)))
        return false;

    beginRemoveRows(QModelIndex(), index, index);
    bool result = projectm_playlist_remove_preset(m_playlist, index);
    endRemoveRows();
    return result;
}

bool QPlaylistModel::removeRows(int row, int count, const QModelIndex & parent)
{
    Q_UNUSED(parent);
    if (!m_playlist || count <= 0)
        return false;

    beginRemoveRows(QModelIndex(), row, row + count - 1);
    uint32_t removed = projectm_playlist_remove_presets(m_playlist, row, count);
    endRemoveRows();
    return removed > 0;
}

void QPlaylistModel::clearItems()
{
    if (!m_playlist)
        return;

    beginResetModel();
    projectm_playlist_clear(m_playlist);
    endResetModel();
}

void QPlaylistModel::clear()
{
    clearItems();
    m_playlistName.clear();
    m_playlistDesc.clear();
}

bool QPlaylistModel::readPlaylist(const QString & file)
{
    // projectM 4.x: Simplified XML reading
    // If no playlist file specified, load from default preset directory
    if (file.isEmpty()) {
        QString presetPath = QDir::homePath() + "/.local/share/projectM/presets";
        QFileInfo presetInfo(presetPath);
        if (presetInfo.exists() && presetInfo.isDir()) {
            qDebug() << "Loading presets from default directory:" << presetPath;
            beginResetModel();
            projectm_playlist_clear(m_playlist);
            projectm_playlist_add_path(m_playlist, presetPath.toLocal8Bit().data(), true, false);
            endResetModel();

            // Start playing the first preset if we loaded any
            uint32_t size = projectm_playlist_size(m_playlist);
            if (size > 0) {
                qDebug() << "Starting playback with" << size << "presets";
                uint32_t pos = projectm_playlist_set_position(m_playlist, 0, true);
                qDebug() << "Playlist position after set:" << pos;

                // Get the preset name to verify it loaded
                char* filename = projectm_playlist_item(m_playlist, pos);
                if (filename) {
                    qDebug() << "First preset:" << QString(filename);
                    projectm_playlist_free_string(filename);
                }
            }
            return true;
        } else {
            qWarning() << "Default preset directory does not exist:" << presetPath;
            return false;
        }
    }

    QFileInfo fileInfo(file);
    if (!fileInfo.exists()) {
        qWarning() << "Playlist file does not exist:" << file;
        // Fall back to default preset directory
        QString presetPath = QDir::homePath() + "/.local/share/projectM/presets";
        QFileInfo presetInfo(presetPath);
        if (presetInfo.exists() && presetInfo.isDir()) {
            qDebug() << "Falling back to default preset directory:" << presetPath;
            beginResetModel();
            projectm_playlist_clear(m_playlist);
            projectm_playlist_add_path(m_playlist, presetPath.toLocal8Bit().data(), true, false);
            endResetModel();

            // Start playing the first preset if we loaded any
            uint32_t size = projectm_playlist_size(m_playlist);
            if (size > 0) {
                qDebug() << "Starting playback with" << size << "presets";
                uint32_t pos = projectm_playlist_set_position(m_playlist, 0, true);
                qDebug() << "Playlist position after set:" << pos;

                // Get the preset name to verify it loaded
                char* filename = projectm_playlist_item(m_playlist, pos);
                if (filename) {
                    qDebug() << "First preset:" << QString(filename);
                    projectm_playlist_free_string(filename);
                }
            }
            return true;
        }
        return false;
    }

    // If the file is a directory, scan it
    if (fileInfo.isDir()) {
        beginResetModel();
        projectm_playlist_clear(m_playlist);
        projectm_playlist_add_path(m_playlist, file.toLocal8Bit().data(), true, false);
        endResetModel();

        // Start playing the first preset if we loaded any
        uint32_t size = projectm_playlist_size(m_playlist);
        if (size > 0) {
            qDebug() << "Starting playback with" << size << "presets from" << file;
            projectm_playlist_set_position(m_playlist, 0, true);
        }
        return true;
    }

    // TODO: Implement XML playlist reading if needed
    qWarning() << "XML playlist reading not yet implemented for projectM 4.x";
    return false;
}

bool QPlaylistModel::writePlaylist(const QString & file)
{
    Q_UNUSED(file);
    // projectM 4.x: XML writing not yet implemented
    qWarning() << "XML playlist writing not yet implemented for projectM 4.x";
    return false;
}

void QPlaylistModel::readPlaylistItem(QXmlStreamReader & reader)
{
    Q_UNUSED(reader);
    // projectM 4.x: Not implemented
}

bool QPlaylistModel::dropMimeData(const QMimeData *data, Qt::DropAction action,
                                  int row, int column, const QModelIndex &parent)
{
    Q_UNUSED(data);
    Q_UNUSED(action);
    Q_UNUSED(row);
    Q_UNUSED(column);
    Q_UNUSED(parent);
    // projectM 4.x: Drag and drop not yet implemented
    return false;
}

void QPlaylistModel::notifyDataChanged(unsigned int index)
{
    if (index < projectm_playlist_size(m_playlist)) {
        QModelIndex idx = this->index(index, 0);
        emit dataChanged(idx, idx);
    }
}

void QPlaylistModel::updateItemHighlights()
{
    // Refresh the entire model to update playing/locked icons
    int rows = rowCount();
    if (rows > 0) {
        emit dataChanged(index(0, 0), index(rows - 1, 0));
    }
}
