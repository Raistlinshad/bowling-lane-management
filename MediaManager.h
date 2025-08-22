#ifndef MEDIAMANAGER_H
#define MEDIAMANAGER_H

#include <QObject>
#include <QMediaPlayer>
#include <QVideoWidget>
#include <QLabel>
#include <QPixmap>
#include <QTimer>
#include <QStackedWidget>
#include <QDir>
#include <QFileInfo>
#include <QStringList>
#include <QRandomGenerator>
#include <QJsonObject>
#include <QJsonArray>
#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QUrl>

// Media item types
enum class MediaType {
    Image,
    Video,
    Effect
};

// Media item class
class MediaItem {
public:
    MediaItem(const QString& filePath, MediaType type, int duration = 0)
        : filePath(filePath), type(type), duration(duration), priority(0) {}
    
    QString filePath;
    MediaType type;
    int duration;       // Duration in seconds (0 = default)
    int priority;       // Higher priority = more likely to be shown
    QStringList tags;   // Tags for categorization
    bool enabled;
    
    QString getName() const {
        return QFileInfo(filePath).baseName();
    }
    
    bool isValid() const {
        return QFileInfo(filePath).exists();
    }
};

// Special effect manager
class EffectManager : public QObject {
    Q_OBJECT
    
public:
    explicit EffectManager(QWidget* parent = nullptr);
    
    void playEffect(const QString& effectName, int duration = 3000);
    void addCustomEffect(const QString& name, const QString& filePath);
    void removeEffect(const QString& name);
    
    QStringList getAvailableEffects() const;

signals:
    void effectStarted(const QString& effectName);
    void effectFinished(const QString& effectName);

private slots:
    void onEffectFinished();

private:
    void createBuiltInEffects();
    void showTextEffect(const QString& text, const QString& color, int duration);
    void showVideoEffect(const QString& filePath, int duration);
    void showImageEffect(const QString& filePath, int duration);
    
    QStackedWidget* effectContainer;
    QTimer* effectTimer;
    QMap<QString, QString> customEffects;  // name -> file path
    QString currentEffect;
};

// Media rotation manager
class MediaRotationManager : public QObject {
    Q_OBJECT
    
public:
    explicit MediaRotationManager(QObject* parent = nullptr);
    
    void addMediaItem(const MediaItem& item);
    void removeMediaItem(const QString& filePath);
    void clearMedia();
    
    void setRotationMode(const QString& mode); // "sequential", "random", "priority"
    void setDefaultImageDuration(int seconds);
    void setDefaultVideoDuration(int seconds);
    
    void startRotation();
    void stopRotation();
    void pauseRotation();
    void resumeRotation();
    
    MediaItem getCurrentItem() const;
    QStringList getMediaList() const;
    
    // Server-controlled media
    void loadServerPlaylist(const QJsonArray& playlist);
    void addServerMedia(const QJsonObject& mediaData);

signals:
    void mediaChanged(const MediaItem& item);
    void rotationStarted();
    void rotationStopped();

private slots:
    void onRotationTimer();
    void selectNextMedia();

private:
    QVector<MediaItem> mediaItems;
    int currentIndex;
    QString rotationMode;
    int defaultImageDuration;
    int defaultVideoDuration;
    bool isRotating;
    bool isPaused;
    
    QTimer* rotationTimer;
    
    MediaItem selectRandomMedia();
    MediaItem selectPriorityMedia();
    MediaItem selectSequentialMedia();
};

// Media download manager
class MediaDownloadManager : public QObject {
    Q_OBJECT
    
public:
    explicit MediaDownloadManager(QObject* parent = nullptr);
    
    void downloadMedia(const QString& url, const QString& localPath);
    void setDownloadDirectory(const QString& directory);
    
    bool isDownloading() const { return activeDownloads > 0; }
    int getActiveDownloads() const { return activeDownloads; }

signals:
    void downloadStarted(const QString& url);
    void downloadProgress(const QString& url, qint64 bytesReceived, qint64 bytesTotal);
    void downloadFinished(const QString& url, const QString& localPath);
    void downloadError(const QString& url, const QString& error);

private slots:
    void onDownloadFinished();
    void onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void onDownloadError(QNetworkReply::NetworkError error);

private:
    QNetworkAccessManager* networkManager;
    QString downloadDirectory;
    int activeDownloads;
    QMap<QNetworkReply*, QString> replyToUrl;
    QMap<QNetworkReply*, QString> replyToPath;
};

// Main media manager class
class MediaManager : public QStackedWidget {
    Q_OBJECT
    
public:
    explicit MediaManager(QWidget* parent = nullptr);
    ~MediaManager();
    
    // Display control
    void showGameDisplay(QWidget* gameWidget);
    void showMediaRotation();
    void showEffect(const QString& effectName, int duration = 3000);
    
    // Media management
    void loadMediaFromDirectory(const QString& directory);
    void addMediaFile(const QString& filePath, MediaType type = MediaType::Image);
    void removeMediaFile(const QString& filePath);
    void clearAllMedia();
    
    // Configuration
    void setImageDuration(int seconds);
    void setRotationMode(const QString& mode);
    void setEffectsEnabled(bool enabled);
    
    // Server communication
    void handleServerMediaCommand(const QJsonObject& command);
    void downloadServerMedia(const QString& url, const QString& filename);
    
    // Settings
    void loadSettings(const QJsonObject& settings);
    void saveSettings() const;
    
    // Video format support
    static QStringList getSupportedVideoFormats();
    static QStringList getSupportedImageFormats();
    
    // Current state
    bool isShowingGame() const { return currentIndex() == gameDisplayIndex; }
    bool isShowingMedia() const { return currentIndex() == mediaDisplayIndex; }
    bool isShowingEffect() const { return currentIndex() == effectDisplayIndex; }

signals:
    void mediaDisplayStarted();
    void mediaDisplayStopped();
    void effectStarted(const QString& effectName);
    void effectFinished(const QString& effectName);
    void mediaDownloaded(const QString& filename);
    void mediaError(const QString& error);

private slots:
    void onMediaRotationChanged(const MediaItem& item);
    void onEffectFinished(const QString& effectName);
    void onMediaDownloaded(const QString& url, const QString& localPath);
    void onVideoStateChanged(QMediaPlayer::State state);
    void onVideoError(QMediaPlayer::Error error);

private:
    void setupUI();
    void setupMediaPlayer();
    void createDisplayWidgets();
    
    void displayImage(const QString& filePath);
    void displayVideo(const QString& filePath);
    void returnToGameDisplay();
    
    QString getMediaDirectory() const;
    void scanMediaDirectory();
    bool isValidMediaFile(const QString& filePath) const;
    MediaType getMediaType(const QString& filePath) const;
    
    // UI Components
    QWidget* gameDisplayWidget;
    QLabel* imageDisplayLabel;
    QVideoWidget* videoDisplayWidget;
    
    // Media components
    QMediaPlayer* mediaPlayer;
    EffectManager* effectManager;
    MediaRotationManager* rotationManager;
    MediaDownloadManager* downloadManager;
    
    // Display indices
    int gameDisplayIndex;
    int mediaDisplayIndex;
    int effectDisplayIndex;
    
    // Settings
    QString mediaDirectory;
    bool effectsEnabled;
    int imageDuration;
    QString rotationMode;
    
    // State tracking
    MediaItem currentMediaItem;
    QTimer* returnToGameTimer;
    
    // Supported formats
    static const QStringList supportedVideoFormats;
    static const QStringList supportedImageFormats;
};

#endif // MEDIAMANAGER_H