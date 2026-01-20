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

#ifndef QPROJECTM_WIDGET_HPP
#define QPROJECTM_WIDGET_HPP

#include <iostream>
#include "qprojectm.hpp"
#include <projectM-4/render_opengl.h>
#include <QOpenGLWidget>
#include <QMutex>
#include <QtDebug>
#include <QKeyEvent>
#include <QTimer>
#include <QApplication>
#include <QSettings>

class QProjectMWidget : public QOpenGLWidget
{

		Q_OBJECT        // must include this if you use Qt signals/slots

	public:
		static const int MOUSE_VISIBLE_TIMEOUT_MS = 5000;
		QProjectMWidget ( const QString& config_file, QWidget * parent, QMutex * audioMutex = 0 )
				: QOpenGLWidget ( parent ), m_config_file ( config_file ), m_projectM ( 0 ), m_mouseTimer ( 0 ), m_audioMutex ( audioMutex )
		{
			// projectM 4.x: Request OpenGL 3.3 Core Profile
			QSurfaceFormat format;
			format.setVersion(3, 3);
			format.setProfile(QSurfaceFormat::CoreProfile);
			format.setDepthBufferSize(24);
			format.setStencilBufferSize(8);
			format.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
			format.setSwapInterval(1); // Enable vsync
			setFormat(format);

			m_mouseTimer = new QTimer ( this );

			QSettings settings("projectM", "qprojectM");
			mouseHideTimeoutSeconds =
				settings.value("MouseHideOnTimeout", MOUSE_VISIBLE_TIMEOUT_MS/1000).toInt();

			if (mouseHideTimeoutSeconds > 0)
				m_mouseTimer->start ( mouseHideTimeoutSeconds * 1000);

			connect ( m_mouseTimer, SIGNAL ( timeout() ), this, SLOT ( hideMouse() ) );
			this->setMouseTracking ( true );

		}

		~QProjectMWidget() { destroyProjectM(); }



		void resizeGL ( int w, int h ) override
		{
			// Setup viewport, projection etc
			setup_opengl ( w,h );
			projectm_set_window_size(m_projectM->instance(), static_cast<size_t>(w), static_cast<size_t>(h));
		}

		inline const QString& configFile()
		{
			return m_config_file;
		}

		inline void seizePresetLock()
		{
			m_presetSeizeMutex.lock();
			m_presetWasLocked = projectm_get_preset_locked(qprojectM()->instance());
            projectm_set_preset_locked(qprojectM()->instance(), true);
		}

		inline void releasePresetLock()
		{
		    projectm_set_preset_locked(qprojectM()->instance(),  m_presetWasLocked);
			m_presetSeizeMutex.unlock();
		}


		inline QProjectM * qprojectM() { return m_projectM; }

	protected slots:
		inline void mouseMoveEvent ( QMouseEvent * event )
		{
			Q_UNUSED(event);

			m_mouseTimer->stop();
			QApplication::restoreOverrideCursor();
			if (mouseHideTimeoutSeconds > 0)
				m_mouseTimer->start ( mouseHideTimeoutSeconds*1000 );

		}

		inline void leaveEvent ( QEvent * event )
		{
			Q_UNUSED(event);
			/// @bug testing if this resolves a bug for ubuntu users
			QApplication::restoreOverrideCursor();
		}

	public slots:

		void resetProjectM()
		{
			std::cout << "resetting" << std::endl;
			qDebug() << "reset start";

			emit ( projectM_BeforeDestroy() );

			if ( m_audioMutex )
				m_audioMutex->lock();

			destroyProjectM();

			// Make a new projectM instance and reset the opengl state
			initializeGL();

			// Allow audio thread to continue its business
			if ( m_audioMutex )
			{
				m_audioMutex->unlock();
			}
			qDebug() << "reinit'ed";
		}

		void setAudioMutex ( QMutex * mutex )
		{
			m_audioMutex = mutex;
		}

		void setPresetLock ( int state )
		{
            projectm_set_preset_locked(m_projectM->instance(), static_cast<bool>(state));
			emit ( presetLockChanged ( ( bool ) state ) );
		}

		void setShuffleEnabled ( int state )
		{
            
			emit ( shuffleEnabledChanged ( ( bool ) state ) );
		}

		void mousePressEvent ( QMouseEvent * event )
		{
			Q_UNUSED(event);

			this->setFocus();
		}

		void updateGL()
        {
		    paintGL();
        }

	signals:
		void projectM_Initialized ( QProjectM * );
		void projectM_BeforeDestroy();
		void presetLockChanged ( bool isLocked );
		void shuffleEnabledChanged ( bool isShuffleEnabled );

	private slots:
		void hideMouse()
		{
			if ( this->underMouse() && this->hasFocus() )
				QApplication::setOverrideCursor ( Qt::BlankCursor );
		}
	private:
		QString m_config_file;
		QProjectM * m_projectM;
		void destroyProjectM()
		{

			if ( m_projectM )
			{
				delete ( m_projectM );
				m_projectM = 0;
			}
		}

		QTimer * m_mouseTimer;
		QMutex * m_audioMutex;
		QMutex m_presetSeizeMutex;
		bool m_presetWasLocked;
	protected:


		void keyReleaseEvent ( QKeyEvent * e )
		{
			// TODO: projectM4 removed the key_handler API. Need to implement
			// keyboard shortcuts using direct preset/settings API calls.
			e->ignore();
		}

		void initializeGL() override
		{

		        if (m_projectM == 0) {
			    this->m_projectM = new QProjectM ( m_config_file );
			    projectM_Initialized ( m_projectM );
			}
		}

		void paintGL() override
		{
            projectm_opengl_render_frame(m_projectM->instance());
		}

	private:
		int mouseHideTimeoutSeconds;
		void setup_opengl ( int w, int h )
		{
			// projectM 4.x handles all OpenGL state internally
			// We only need to set the viewport for the OpenGL 3.3 Core Profile
			glViewport ( 0, 0, w, h );
		}


};
#endif
