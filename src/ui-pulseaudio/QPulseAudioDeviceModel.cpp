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

#include "QPulseAudioDeviceModel.hpp"
#include <QtWidgets>
#include <QIcon>
#include <QtDebug>
#include <QMessageBox>
#include <QHash>

QPulseAudioDeviceModel::QPulseAudioDeviceModel(const QHash<int, QString> & devices, const QHash<int,QString>::const_iterator & devicePosition, QObject * parent): QAbstractListModel(parent), _devices(devices), _devicePosition(devicePosition) {

}

void QPulseAudioDeviceModel::updateItemHighlights()
{
	if ( rowCount() == 0 )
		return;

	emit ( dataChanged ( this->index (0), this->index ( rowCount()-1 ) ));
}

QVariant QPulseAudioDeviceModel::data ( const QModelIndex & index, int role = Qt::DisplayRole ) const
{
	if (!index.isValid())
         return QVariant();

	if (index.row() >= rowCount())
		return QVariant();

	switch ( role )
	{
		case Qt::DisplayRole:
			return _devices[index.row()];
		case Qt::DecorationRole:
		{
			QIcon icon(":/check.png");
			return icon;
			break;

		}

		case Qt::ToolTipRole:
			if (_devicePosition.key() == index.row())
 				return _devices[index.row()] + " (active)";
			else
				return _devices[index.row()] + " (inactive)";


		case Qt::BackgroundRole:
			if (_devicePosition.key() == index.row()) {
                // Use bright green for maximum visibility
                QColor highlight(0, 255, 0, 180);  // Bright green, mostly opaque
                return highlight;
			}
			// Don't set background for inactive items - let theme handle it
			return QVariant();

		case Qt::ForegroundRole:
			if (_devicePosition.key() == index.row()) {
				// Black text on bright green for maximum contrast and visibility
				return QColor(Qt::black);
			}
			// White text for inactive items
			return QColor(Qt::white);

		case Qt::FontRole:
			if (_devicePosition.key() == index.row()) {
				// Make active device bold
				QFont boldFont;
				boldFont.setBold(true);
				boldFont.setPointSize(10);
				return boldFont;
			}
			return QVariant();

		default:

			return QVariant();
	}
}

int QPulseAudioDeviceModel::rowCount ( const QModelIndex & parent) const
{
	Q_UNUSED(parent);

	return _devices.count();
}



