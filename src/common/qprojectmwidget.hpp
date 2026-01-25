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
#include <QMutexLocker>
#include <QtDebug>
#include <QKeyEvent>
#include <QTimer>
#include <QApplication>
#include <QSettings>
#include <vector>
#include <cstring>
#include <algorithm>

class QProjectMWidget : public QOpenGLWidget
{

		Q_OBJECT        // must include this if you use Qt signals/slots

	public:
		static const int MOUSE_VISIBLE_TIMEOUT_MS = 5000;
		QProjectMWidget ( const QString& config_file, QWidget * parent, QMutex * audioMutex = 0 )
				: QOpenGLWidget ( parent ), m_config_file ( config_file ), m_projectM ( 0 ), m_mouseTimer ( 0 ), m_renderTimer ( 0 ), m_audioMutex ( audioMutex ),
				  m_audioBuffer(AUDIO_BUFFER_SIZE, 0.0f), m_audioWritePos(0), m_audioReadPos(0)
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

			// Create render timer for continuous animation
			m_renderTimer = new QTimer ( this );
			m_renderTimer->setInterval(16); // ~60 FPS
			connect ( m_renderTimer, SIGNAL ( timeout() ), this, SLOT ( triggerUpdate() ) );
			qDebug() << "Render timer created and starting...";
			m_renderTimer->start();
			qDebug() << "Render timer running:" << m_renderTimer->isActive();

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
			qDebug() << "resizeGL called:" << w << "x" << h;
			// Setup viewport, projection etc
			setup_opengl ( w,h );
			if (m_projectM && m_projectM->instance()) {
			    projectm_set_window_size(m_projectM->instance(), static_cast<size_t>(w), static_cast<size_t>(h));
			    qDebug() << "resizeGL: Window size set in projectM";
			}
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

		// Thread-safe audio queueing - called from audio thread
		void queueAudio(const float* samples, size_t count) {
			QMutexLocker locker(&m_audioBufferMutex);
			for (size_t i = 0; i < count && i < AUDIO_BUFFER_SIZE; ++i) {
				m_audioBuffer[m_audioWritePos] = samples[i];
				m_audioWritePos = (m_audioWritePos + 1) % AUDIO_BUFFER_SIZE;
			}
		}

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

		void triggerUpdate()
		{
			static int triggerCount = 0;
			if (triggerCount % 60 == 0) {
				qDebug() << "triggerUpdate called" << triggerCount << "times, isVisible:" << isVisible();
			}
			triggerCount++;
			// Use repaint() to force immediate paint, not just schedule it
			repaint();
		}

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
		    static int updateCount = 0;
		    if (updateCount % 60 == 0) {
		        qDebug() << "updateGL called" << updateCount << "times";
		    }
		    updateCount++;
		    update(); // Trigger repaint
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
		QTimer * m_renderTimer;
		QMutex * m_audioMutex;
		QMutex m_presetSeizeMutex;
		bool m_presetWasLocked;

		// Thread-safe audio buffer for visualization
		static constexpr size_t AUDIO_BUFFER_SIZE = 16384;
		std::vector<float> m_audioBuffer;
		size_t m_audioWritePos;
		size_t m_audioReadPos;
		QMutex m_audioBufferMutex;
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
		    static int frameCount = 0;
		    if (!m_projectM || !m_projectM->instance()) {
		        qDebug() << "paintGL: No projectM instance";
		        return;
		    }

		    if (frameCount == 0) {
		        qDebug() << "paintGL: First frame, widget size:" << width() << "x" << height();
		        qDebug() << "paintGL: Calling projectm_opengl_render_frame";

		        // Check OpenGL state
		        GLint viewport[4];
		        glGetIntegerv(GL_VIEWPORT, viewport);
		        qDebug() << "paintGL: Viewport:" << viewport[0] << viewport[1] << viewport[2] << viewport[3];

		        GLint fbo;
		        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &fbo);
		        qDebug() << "paintGL: Current FBO:" << fbo;
		    }

		    // Process buffered audio in render thread (thread-safe)
		    {
		        QMutexLocker locker(&m_audioBufferMutex);
		        if (m_audioReadPos != m_audioWritePos) {
		            // Calculate available samples
		            size_t available = (m_audioWritePos - m_audioReadPos + AUDIO_BUFFER_SIZE) % AUDIO_BUFFER_SIZE;
		            // Process in chunks
		            static std::vector<float> tempBuffer(4096);
		            while (available > 0) {
		                size_t toRead = std::min(available, tempBuffer.size());
		                for (size_t i = 0; i < toRead; ++i) {
		                    tempBuffer[i] = m_audioBuffer[m_audioReadPos];
		                    m_audioReadPos = (m_audioReadPos + 1) % AUDIO_BUFFER_SIZE;
		                }
		                // Add audio to projectM (now in render thread - safe!)
		                projectm_pcm_add_float(m_projectM->instance(), tempBuffer.data(),
		                                       static_cast<unsigned int>(toRead / 2), PROJECTM_STEREO);
		                available -= toRead;
		            }
		        }
		    }

		    // QOpenGLWidget uses its own FBO - we must render to it, not FBO 0
		    GLuint fbo = defaultFramebufferObject();
		    projectm_opengl_render_frame_fbo(m_projectM->instance(), fbo);

		    if (frameCount == 0) {
		        GLenum err = glGetError();
		        if (err != GL_NO_ERROR) {
		            qDebug() << "paintGL: OpenGL error after render:" << err;
		        }
		        qDebug() << "paintGL: First frame complete";
		    }

		    frameCount++;

		    // Log every 60 frames (~1 second at 60fps)
		    if (frameCount % 60 == 0) {
		        qDebug() << "paintGL: Frame" << frameCount << "rendered";
		    }
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
