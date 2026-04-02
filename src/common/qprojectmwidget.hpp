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

#include <functional>
#include "qprojectm.hpp"
#include <projectM-4/render_opengl.h>
#include <QOpenGLWidget>
#include <QMutex>
#include <QMutexLocker>
#include <QtDebug>
#include <QKeyEvent>
#include <QTimer>
#include <QElapsedTimer>
#include <QApplication>
#include <QSettings>
#include <QPainter>
#include <QFontMetrics>
#include <vector>
#include <queue>
#include <cstring>
#include <algorithm>

class QProjectMWidget : public QOpenGLWidget
{

		Q_OBJECT        // must include this if you use Qt signals/slots

	public:
		static const int MOUSE_VISIBLE_TIMEOUT_MS = 5000;
		static constexpr qint64 OVERLAY_HOLD_MS = 4000;
		static constexpr qint64 OVERLAY_FADE_MS = 1500;
		QProjectMWidget ( const QString& config_file, QWidget * parent, QMutex * audioMutex = 0 )
				: QOpenGLWidget ( parent ), m_config_file ( config_file ), m_projectM ( 0 ), m_mouseTimer ( 0 ), m_renderTimer ( 0 ), m_audioMutex ( audioMutex ),
				  m_audioBuffer(AUDIO_BUFFER_SIZE, 0.0f), m_audioWritePos(0), m_audioReadPos(0)
		{
			// Use the application-wide default format set in main().
			// Do NOT override with setFormat() here — mismatched formats
			// cause EGL_BAD_MATCH (error 3009) on NVIDIA Wayland.
			setAttribute(Qt::WA_OpaquePaintEvent);
			setAttribute(Qt::WA_NoSystemBackground);

			m_mouseTimer = new QTimer ( this );

			// Create render timer for continuous animation
			m_renderTimer = new QTimer ( this );
			m_renderTimer->setInterval(16); // ~60 FPS
			connect ( m_renderTimer, SIGNAL ( timeout() ), this, SLOT ( triggerUpdate() ) );
			m_renderTimer->start();

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
			// Queue resize for next paintGL to avoid race conditions
			m_pendingWidth = w;
			m_pendingHeight = h;
			m_resizePending = true;
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

		// Get mutex for protecting projectM API calls from other threads
		QMutex* projectMMutex() { return &m_projectMMutex; }

		// Thread-safe audio queueing - called from audio thread
		void queueAudio(const float* samples, size_t count) {
			QMutexLocker locker(&m_audioBufferMutex);
			for (size_t i = 0; i < count && i < AUDIO_BUFFER_SIZE; ++i) {
				m_audioBuffer[m_audioWritePos] = samples[i];
				m_audioWritePos = (m_audioWritePos + 1) % AUDIO_BUFFER_SIZE;
			}
		}

		// Queue an operation that requires the OpenGL context to be current.
		// The operation will execute inside paintGL() before rendering.
		// Use this for preset switching, window resize, and any projectM API
		// call that creates/destroys GL resources (shaders, textures, etc).
		void queueGLOperation(std::function<void()> op) {
			QMutexLocker locker(&m_glOpsMutex);
			m_pendingGLOps.push(std::move(op));
		}

		void showPresetOverlay(const QString& name) {
			m_overlayText = name;
			m_overlayOpacity = 1.0;
			m_overlayTimer.start();
		}

		void setOverlayEnabled(bool enabled) {
			m_overlayEnabled = enabled;
		}

		bool overlayEnabled() const {
			return m_overlayEnabled;
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
			update();
		}

		void resetProjectM()
		{
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

		void mousePressEvent ( QMouseEvent * event )
		{
			Q_UNUSED(event);

			this->setFocus();
		}

		void updateGL()
		{
			update();
		}

	signals:
		void projectM_Initialized ( QProjectM * );
		void projectM_BeforeDestroy();
		void presetLockChanged ( bool isLocked );

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
		bool m_presetWasLocked = false;

		// Thread-safe audio buffer for visualization
		static constexpr size_t AUDIO_BUFFER_SIZE = 16384;
		std::vector<float> m_audioBuffer;
		size_t m_audioWritePos;
		size_t m_audioReadPos;
		QMutex m_audioBufferMutex;

		// Mutex to protect all projectM API calls
		QMutex m_projectMMutex;

		// Deferred resize to avoid race conditions
		int m_pendingWidth = 0;
		int m_pendingHeight = 0;
		bool m_resizePending = false;

		// Queue of operations that need the GL context current
		std::queue<std::function<void()>> m_pendingGLOps;
		QMutex m_glOpsMutex;

		// Preset name overlay state
		QString m_overlayText;
		qreal m_overlayOpacity = 0.0;
		QElapsedTimer m_overlayTimer;
		bool m_overlayEnabled = true;
	protected:


		void keyReleaseEvent ( QKeyEvent * e )
		{
			// projectM 4.x: Key handling moved to QProjectM_MainWindow::keyReleaseEvent
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
		    if (!m_projectM || !m_projectM->instance()) {
		        return;
		    }

		    // Lock projectM mutex to protect all API calls
		    QMutexLocker projectMLock(&m_projectMMutex);

		    // Process any pending resize first (deferred from resizeGL)
		    if (m_resizePending) {
		        projectm_set_window_size(m_projectM->instance(),
		                                 static_cast<size_t>(m_pendingWidth),
		                                 static_cast<size_t>(m_pendingHeight));
		        m_resizePending = false;
		    }

		    // Process deferred GL operations (preset switches, etc.)
		    // GL context is guaranteed current here inside paintGL.
		    // This is critical: preset loading compiles shaders and creates
		    // GL textures, which requires an active GL context.
		    {
		        QMutexLocker locker(&m_glOpsMutex);
		        while (!m_pendingGLOps.empty()) {
		            auto op = std::move(m_pendingGLOps.front());
		            m_pendingGLOps.pop();
		            locker.unlock();
		            op();
		            locker.relock();
		        }
		    }

		    // Process buffered audio in render thread
		    {
		        QMutexLocker locker(&m_audioBufferMutex);
		        if (m_audioReadPos != m_audioWritePos) {
		            size_t available = (m_audioWritePos - m_audioReadPos + AUDIO_BUFFER_SIZE) % AUDIO_BUFFER_SIZE;
		            constexpr size_t MAX_CHUNK = 2048;
		            float tempBuffer[MAX_CHUNK];
		            size_t toRead = std::min(available, MAX_CHUNK);
		            toRead &= ~size_t(1); // Ensure even count for stereo frame alignment
		            for (size_t i = 0; i < toRead; ++i) {
		                float sample = m_audioBuffer[m_audioReadPos];
		                tempBuffer[i] = std::max(-1.0f, std::min(1.0f, sample));
		                m_audioReadPos = (m_audioReadPos + 1) % AUDIO_BUFFER_SIZE;
		            }
		            projectm_pcm_add_float(m_projectM->instance(), tempBuffer,
		                                   static_cast<unsigned int>(toRead / 2), PROJECTM_STEREO);
		        }
		    }

		    // Determine if we need the overlay this frame
		    bool drawOverlay = m_overlayEnabled && m_overlayOpacity > 0.0;
		    if (drawOverlay) {
		        qint64 elapsed = m_overlayTimer.elapsed();
		        if (elapsed < OVERLAY_HOLD_MS) {
		            m_overlayOpacity = 1.0;
		        } else if (elapsed < OVERLAY_HOLD_MS + OVERLAY_FADE_MS) {
		            m_overlayOpacity = 1.0 - (elapsed - OVERLAY_HOLD_MS) / (qreal)OVERLAY_FADE_MS;
		        } else {
		            m_overlayOpacity = 0.0;
		        }
		        drawOverlay = m_overlayOpacity > 0.0;
		    }

		    if (drawOverlay) {
		        // Use QPainter to bracket native GL so state is properly
		        // saved/restored for 2D drawing after projectM renders.
		        QPainter painter(this);
		        painter.beginNativePainting();

		        GLuint fbo = defaultFramebufferObject();
		        projectm_opengl_render_frame_fbo(m_projectM->instance(), fbo);

		        painter.endNativePainting();

		        // QPainter 2D overlay on top of the GL content
		        painter.setRenderHint(QPainter::Antialiasing);
		        QFont font("Sans", 14);
		        painter.setFont(font);
		        QFontMetrics fm(font);
		        QRect textRect = fm.boundingRect(m_overlayText);
		        int pad = 12;
		        QRect bg(width()/2 - textRect.width()/2 - pad,
		                 height() - 60,
		                 textRect.width() + pad*2,
		                 textRect.height() + pad);
		        painter.setOpacity(m_overlayOpacity * 0.7);
		        painter.setBrush(QColor(0, 0, 0));
		        painter.setPen(Qt::NoPen);
		        painter.drawRoundedRect(bg, 8, 8);
		        painter.setOpacity(m_overlayOpacity);
		        painter.setPen(Qt::white);
		        painter.drawText(bg, Qt::AlignCenter, m_overlayText);
		        painter.end();
		    } else {
		        // No overlay needed - just render
		        GLuint fbo = defaultFramebufferObject();
		        projectm_opengl_render_frame_fbo(m_projectM->instance(), fbo);
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
