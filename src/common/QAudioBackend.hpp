#ifndef QAUDIO_BACKEND_HPP
#define QAUDIO_BACKEND_HPP

#include <QObject>
#include <QString>
#include <QList>

class QProjectM_MainWindow;
class QMutex;

class QAudioBackend : public QObject
{
    Q_OBJECT

public:
    struct DeviceInfo {
        QString id;
        QString displayName;
    };

    explicit QAudioBackend(QObject *parent = nullptr) : QObject(parent) {}
    virtual ~QAudioBackend() {}

    virtual bool start(QProjectM_MainWindow *mainWindow, QMutex *audioMutex) = 0;
    virtual void stop() = 0;

    virtual QString backendName() const = 0;

    virtual QList<DeviceInfo> devices() const = 0;
    virtual QString currentDeviceId() const = 0;
    virtual bool supportsDeviceSwitching() const = 0;

    virtual void writeSettings() = 0;
    virtual void readSettings() = 0;

public slots:
    virtual void selectDevice(const QString &deviceId) = 0;

signals:
    void devicesChanged();
    void activeDeviceChanged();
    void errorOccurred(const QString &message);
};

#endif // QAUDIO_BACKEND_HPP
