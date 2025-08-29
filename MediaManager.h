#ifndef MEDIAMANAGER_H
#define MEDIAMANAGER_H

#include <QStackedWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QTimer>
#include <QWidget>
#include <QJsonObject>

// Forward declarations for multimedia types (when not available)
#ifndef MULTIMEDIA_SUPPORT
enum class MediaPlayerState { PlayingState, PausedState, StoppedState };
enum class MediaPlayerError { NoError, ResourceError, FormatError, NetworkError };

class QMediaPlayer : public QObject {
    Q_OBJECT
public:
    using State = MediaPlayerState;
    using Error = MediaPlayerError;
    QMediaPlayer(QObject* parent = nullptr) : QObject(parent) {}
    void setSource(const QString&) {}
    void play() {}
    void pause() {}
    void stop() {}
signals:
    void stateChanged(State state);
    void errorOccurred(Error error);
};

class QVideoWidget : public QWidget {
    Q_OBJECT
public:
    QVideoWidget(QWidget* parent = nullptr) : QWidget(parent) {}
};
#else
#include <QMediaPlayer>
#include <QVideoWidget>
#endif

class MediaManager : public QStackedWidget {
    Q_OBJECT
    
public:
    explicit MediaManager(QWidget* parent = nullptr);
    void showGameDisplay(QWidget* gameWidget);
    void showEffect(const QString& effect, int duration = 2000);
    void showMediaRotation();
    void loadSettings(const QJsonObject& settings);

signals:
    void effectStarted(const QString& effect);
    void effectFinished(const QString& effect);

private slots:
    void onEffectTimer();
    void onVideoStateChanged(QMediaPlayer::State state);
    void onVideoError(QMediaPlayer::Error error);

private:
    void setupUI();
    void createStubDisplay();
    
    // UI Components
    QWidget* stubDisplayWidget;
    QLabel* stubLabel;
    QWidget* gameDisplayWidget;
    QVideoWidget* videoDisplayWidget;
    QMediaPlayer* mediaPlayer;
    
    // Timers and state
    QTimer* effectTimer;
    QString currentEffect;
    bool isGameMode;
    QJsonObject settings;
};

#endif // MEDIAMANAGER_H