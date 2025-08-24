#include "MediaManager.h"
#include <QDebug>
#include <QApplication>
#include <QStandardPaths>
#include <QImageReader>
#include <QNetworkRequest>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QSettings>
#include <QVBoxLayout>

// Static constants
const QStringList MediaManager::supportedVideoFormats = {"mp4", "avi", "mov", "mkv", "wmv", "flv"};
const QStringList MediaManager::supportedImageFormats = {"jpg", "jpeg", "png", "bmp", "gif", "tiff"};

// EffectManager implementation
EffectManager::EffectManager(QWidget* parent) : QObject(parent) {
    effectContainer = new QStackedWidget(parent);
    effectTimer = new QTimer(this);
    effectTimer->setSingleShot(true);
    connect(effectTimer, &QTimer::timeout, this, &EffectManager::onEffectFinished);
    
    createBuiltInEffects();
}

void EffectManager::playEffect(const QString& effectName, int duration) {
    qDebug() << "Playing effect:" << effectName << "for" << duration << "ms";
    
    currentEffect = effectName;
    emit effectStarted(effectName);
    
    if (effectName == "strike") {
        showTextEffect("STRIKE!", "red", duration);
    } else if (effectName == "spare") {
        showTextEffect("SPARE!", "blue", duration);
    } else if (customEffects.contains(effectName)) {
        QString filePath = customEffects[effectName];
        if (filePath.endsWith(".mp4") || filePath.endsWith(".avi")) {
            showVideoEffect(filePath, duration);
        } else {
            showImageEffect(filePath, duration);
        }
    } else {
        showTextEffect(effectName.toUpper(), "yellow", duration);
    }
    
    effectTimer->start(duration);
}

void EffectManager::addCustomEffect(const QString& name, const QString& filePath) {
    if (QFileInfo(filePath).exists()) {
        customEffects[name] = filePath;
        qDebug() << "Added custom effect:" << name << "->" << filePath;
    }
}

void EffectManager::removeEffect(const QString& name) {
    customEffects.remove(name);
}

QStringList EffectManager::getAvailableEffects() const {
    QStringList effects = {"strike", "spare"};
    effects << customEffects.keys();
    return effects;
}

void EffectManager::onEffectFinished() {
    emit effectFinished(currentEffect);
    currentEffect.clear();
}

void EffectManager::createBuiltInEffects() {
    // Built-in effects are created dynamically
}

void EffectManager::showTextEffect(const QString& text, const QString& color, int duration) {
    Q_UNUSED(duration)
    
    QLabel* effectLabel = new QLabel(text, effectContainer);
    effectLabel->setAlignment(Qt::AlignCenter);
    effectLabel->setStyleSheet(QString(
        "QLabel { "
        "background-color: %1; "
        "color: white; "
        "font-size: 72px; "
        "font-weight: bold; "
        "border: 5px solid white; "
        "border-radius: 20px; "
        "}"
    ).arg(color));
    
    effectContainer->addWidget(effectLabel);
    effectContainer->setCurrentWidget(effectLabel);
    
    // Remove label after use
    QTimer::singleShot(duration + 1000, effectLabel, &QLabel::deleteLater);
}

void EffectManager::showVideoEffect(const QString& filePath, int duration) {
    Q_UNUSED(filePath)
    Q_UNUSED(duration)
    // Video effects would require QMediaPlayer integration
    // For now, fall back to text
    showTextEffect("VIDEO EFFECT", "purple", duration);
}

void EffectManager::showImageEffect(const QString& filePath, int duration) {
    Q_UNUSED(duration)
    
    QLabel* imageLabel = new QLabel(effectContainer);
    QPixmap pixmap(filePath);
    if (!pixmap.isNull()) {
        imageLabel->setPixmap(pixmap.scaled(effectContainer->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
        imageLabel->setAlignment(Qt::AlignCenter);
        
        effectContainer->addWidget(imageLabel);
        effectContainer->setCurrentWidget(imageLabel);
        
        // Remove label after use
        QTimer::singleShot(duration + 1000, imageLabel, &QLabel::deleteLater);
    }
}

// MediaRotationManager implementation
MediaRotationManager::MediaRotationManager(QObject* parent) 
    : QObject(parent), currentIndex(0), rotationMode("sequential"), 
      defaultImageDuration(300), defaultVideoDuration(0), 
      isRotating(false), isPaused(false) {
    
    rotationTimer = new QTimer(this);
    connect(rotationTimer, &QTimer::timeout, this, &MediaRotationManager::onRotationTimer);
}

void MediaRotationManager::addMediaItem(const MediaItem& item) {
    if (item.isValid()) {
        mediaItems.append(item);
        qDebug() << "Added media item:" << item.filePath;
    }
}

void MediaRotationManager::removeMediaItem(const QString& filePath) {
    for (int i = 0; i < mediaItems.size(); ++i) {
        if (mediaItems[i].filePath == filePath) {
            mediaItems.removeAt(i);
            if (currentIndex >= i && currentIndex > 0) {
                currentIndex--;
            }
            break;
        }
    }
}

void MediaRotationManager::clearMedia() {
    mediaItems.clear();
    currentIndex = 0;
}

void MediaRotationManager::setRotationMode(const QString& mode) {
    rotationMode = mode;
    qDebug() << "Rotation mode set to:" << mode;
}

void MediaRotationManager::setDefaultImageDuration(int seconds) {
    defaultImageDuration = seconds;
}

void MediaRotationManager::setDefaultVideoDuration(int seconds) {
    defaultVideoDuration = seconds;
}

void MediaRotationManager::startRotation() {
    if (!mediaItems.isEmpty() && !isRotating) {
        isRotating = true;
        isPaused = false;
        selectNextMedia();
        emit rotationStarted();
    }
}

void MediaRotationManager::stopRotation() {
    isRotating = false;
    isPaused = false;
    rotationTimer->stop();
    emit rotationStopped();
}

void MediaRotationManager::pauseRotation() {
    isPaused = true;
    rotationTimer->stop();
}

void MediaRotationManager::resumeRotation() {
    if (isRotating && isPaused) {
        isPaused = false;
        onRotationTimer(); // Resume immediately
    }
}

MediaItem MediaRotationManager::getCurrentItem() const {
    if (currentIndex >= 0 && currentIndex < mediaItems.size()) {
        return mediaItems[currentIndex];
    }
    return MediaItem("", MediaType::Image);
}

QStringList MediaRotationManager::getMediaList() const {
    QStringList list;
    for (const MediaItem& item : mediaItems) {
        list << item.filePath;
    }
    return list;
}

void MediaRotationManager::loadServerPlaylist(const QJsonArray& playlist) {
    clearMedia();
    
    for (const QJsonValue& value : playlist) {
        QJsonObject itemObj = value.toObject();
        QString filePath = itemObj["file_path"].toString();
        QString typeStr = itemObj["type"].toString();
        int duration = itemObj["duration"].toInt();
        int priority = itemObj["priority"].toInt();
        
        MediaType type = MediaType::Image;
        if (typeStr == "video") type = MediaType::Video;
        else if (typeStr == "effect") type = MediaType::Effect;
        
        MediaItem item(filePath, type, duration);
        item.priority = priority;
        
        QJsonArray tagsArray = itemObj["tags"].toArray();
        for (const QJsonValue& tagValue : tagsArray) {
            item.tags << tagValue.toString();
        }
        
        addMediaItem(item);
    }
}

void MediaRotationManager::addServerMedia(const QJsonObject& mediaData) {
    QString filePath = mediaData["file_path"].toString();
    QString typeStr = mediaData["type"].toString();
    int duration = mediaData["duration"].toInt();
    int priority = mediaData["priority"].toInt();
    
    MediaType type = MediaType::Image;
    if (typeStr == "video") type = MediaType::Video;
    else if (typeStr == "effect") type = MediaType::Effect;
    
    MediaItem item(filePath, type, duration);
    item.priority = priority;
    
    addMediaItem(item);
}

void MediaRotationManager::onRotationTimer() {
    if (!isRotating || isPaused) return;
    
    selectNextMedia();
}

void MediaRotationManager::selectNextMedia() {
    if (mediaItems.isEmpty()) return;
    
    MediaItem nextItem("", MediaType::Image, 0);
    
    if (rotationMode == "random") {
        nextItem = selectRandomMedia();
    } else if (rotationMode == "priority") {
        nextItem = selectPriorityMedia();
    } else {
        nextItem = selectSequentialMedia();
    }
    
    emit mediaChanged(nextItem);
    
    // Set timer for next rotation
    int duration = nextItem.duration;
    if (duration == 0) {
        if (nextItem.type == MediaType::Image) {
            duration = defaultImageDuration;
        } else {
            duration = defaultVideoDuration;
        }
    }
    
    if (duration > 0) {
        rotationTimer->start(duration * 1000);
    }
}

MediaItem MediaRotationManager::selectRandomMedia() {
    if (mediaItems.isEmpty()) return MediaItem("", MediaType::Image);
    
    int randomIndex = QRandomGenerator::global()->bounded(mediaItems.size());
    currentIndex = randomIndex;
    return mediaItems[currentIndex];
}

MediaItem MediaRotationManager::selectPriorityMedia() {
    if (mediaItems.isEmpty()) return MediaItem("", MediaType::Image);
    
    // Create weighted selection based on priority
    QVector<int> weights;
    int totalWeight = 0;
    
    for (const MediaItem& item : mediaItems) {
        int weight = qMax(1, item.priority);
        weights.append(weight);
        totalWeight += weight;
    }
    
    int randomValue = QRandomGenerator::global()->bounded(totalWeight);
    int accumulatedWeight = 0;
    
    for (int i = 0; i < weights.size(); ++i) {
        accumulatedWeight += weights[i];
        if (randomValue < accumulatedWeight) {
            currentIndex = i;
            return mediaItems[i];
        }
    }
    
    // Fallback
    currentIndex = 0;
    return mediaItems[0];
}

MediaItem MediaRotationManager::selectSequentialMedia() {
    if (mediaItems.isEmpty()) return MediaItem("", MediaType::Image);
    
    currentIndex = (currentIndex + 1) % mediaItems.size();
    return mediaItems[currentIndex];
}

// MediaDownloadManager implementation
MediaDownloadManager::MediaDownloadManager(QObject* parent) 
    : QObject(parent), activeDownloads(0) {
    networkManager = new QNetworkAccessManager(this);
    downloadDirectory = "media/downloads";
}

void MediaDownloadManager::downloadMedia(const QString& url, const QString& localPath) {
    QNetworkRequest request{QUrl(url)};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    
    QNetworkReply* reply = networkManager->get(request);
    
    connect(reply, &QNetworkReply::finished, this, &MediaDownloadManager::onDownloadFinished);
    connect(reply, &QNetworkReply::downloadProgress, this, &MediaDownloadManager::onDownloadProgress);
    connect(reply, QOverload<QNetworkReply::NetworkError>::of(&QNetworkReply::errorOccurred),
            this, &MediaDownloadManager::onDownloadError);
    
    replyToUrl[reply] = url;
    replyToPath[reply] = localPath;
    activeDownloads++;
    
    emit downloadStarted(url);
}

void MediaDownloadManager::setDownloadDirectory(const QString& directory) {
    downloadDirectory = directory;
    QDir().mkpath(directory);
}

void MediaDownloadManager::onDownloadFinished() {
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    
    QString url = replyToUrl.value(reply);
    QString localPath = replyToPath.value(reply);
    
    if (reply->error() == QNetworkReply::NoError) {
        // Save the downloaded data
        QFile file(localPath);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(reply->readAll());
            file.close();
            emit downloadFinished(url, localPath);
        } else {
            emit downloadError(url, "Failed to save file: " + localPath);
        }
    }
    
    replyToUrl.remove(reply);
    replyToPath.remove(reply);
    activeDownloads--;
    
    reply->deleteLater();
}

void MediaDownloadManager::onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal) {
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    
    QString url = replyToUrl.value(reply);
    emit downloadProgress(url, bytesReceived, bytesTotal);
}

void MediaDownloadManager::onDownloadError(QNetworkReply::NetworkError error) {
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    
    QString url = replyToUrl.value(reply);
    emit downloadError(url, reply->errorString());
}

// MediaManager main class implementation
MediaManager::MediaManager(QWidget* parent) 
    : QStackedWidget(parent), gameDisplayIndex(-1), mediaDisplayIndex(-1), 
      effectDisplayIndex(-1), effectsEnabled(true), imageDuration(300), 
      rotationMode("sequential"), currentMediaItem("", MediaType::Image, 0) {
    
    setupUI();
    setupMediaPlayer();
    
    effectManager = new EffectManager(this);
    rotationManager = new MediaRotationManager(this);
    downloadManager = new MediaDownloadManager(this);
    
    connect(rotationManager, &MediaRotationManager::mediaChanged, 
            this, &MediaManager::onMediaRotationChanged);
    connect(effectManager, &EffectManager::effectFinished,
            this, &MediaManager::onEffectFinished);
    connect(downloadManager, &MediaDownloadManager::downloadFinished,
            this, &MediaManager::onMediaDownloaded);
    
    returnToGameTimer = new QTimer(this);
    returnToGameTimer->setSingleShot(true);
    connect(returnToGameTimer, &QTimer::timeout, this, &MediaManager::returnToGameDisplay);
    
    mediaDirectory = getMediaDirectory();
    scanMediaDirectory();
}

MediaManager::~MediaManager() {
    rotationManager->stopRotation();
}

void MediaManager::setupUI() {
    // Create display widgets
    createDisplayWidgets();
}

void MediaManager::setupMediaPlayer() {
    mediaPlayer = new QMediaPlayer(this, QMediaPlayer::VideoSurface);
    
    connect(mediaPlayer, &QMediaPlayer::stateChanged,
            this, &MediaManager::onVideoStateChanged);
    connect(mediaPlayer, QOverload<QMediaPlayer::Error>::of(&QMediaPlayer::error),
            this, &MediaManager::onVideoError);
}

void MediaManager::createDisplayWidgets() {
    // Game display widget (placeholder)
    gameDisplayWidget = new QWidget(this);
    gameDisplayWidget->setStyleSheet("QWidget { background-color: blue; }");
    QLabel* gameLabel = new QLabel("Game Display Area", gameDisplayWidget);
    gameLabel->setAlignment(Qt::AlignCenter);
    gameLabel->setStyleSheet("QLabel { color: white; font-size: 24px; }");
    QVBoxLayout* gameLayout = new QVBoxLayout(gameDisplayWidget);
    gameLayout->addWidget(gameLabel);
    gameDisplayIndex = addWidget(gameDisplayWidget);
    
    // Image display widget
    imageDisplayLabel = new QLabel(this);
    imageDisplayLabel->setAlignment(Qt::AlignCenter);
    imageDisplayLabel->setStyleSheet("QLabel { background-color: black; }");
    imageDisplayLabel->setScaledContents(true);
    mediaDisplayIndex = addWidget(imageDisplayLabel);
    
    // Video display widget
    videoDisplayWidget = new QVideoWidget(this);
    videoDisplayWidget->setStyleSheet("QVideoWidget { background-color: black; }");
    addWidget(videoDisplayWidget);
    
    // Effect display (uses the effect manager's container)
    // effectDisplayIndex = addWidget(effectManager->effectContainer);
    effectDisplayIndex = addWidget(new QLabel("Effects Disabled", this));
    
    // Start with game display
    setCurrentIndex(gameDisplayIndex);
}

void MediaManager::showGameDisplay(QWidget* gameWidget) {
    if (gameWidget && gameDisplayIndex >= 0) {
        // Replace the placeholder game widget
        QWidget* oldWidget = widget(gameDisplayIndex);
        removeWidget(oldWidget);
        
        gameDisplayIndex = insertWidget(gameDisplayIndex, gameWidget);
        setCurrentIndex(gameDisplayIndex);
        
        oldWidget->deleteLater();
    }
    
    rotationManager->stopRotation();
    emit mediaDisplayStopped();
}

void MediaManager::showMediaRotation() {
    if (!rotationManager->getMediaList().isEmpty()) {
        rotationManager->startRotation();
        emit mediaDisplayStarted();
    }
}

void MediaManager::showEffect(const QString& effectName, int duration) {
    if (!effectsEnabled) return;
    
    setCurrentIndex(effectDisplayIndex);
    effectManager->playEffect(effectName, duration);
    
    // Return to previous display after effect
    returnToGameTimer->start(duration + 500);
    
    emit effectStarted(effectName);
}

void MediaManager::loadMediaFromDirectory(const QString& directory) {
    mediaDirectory = directory;
    scanMediaDirectory();
}

void MediaManager::addMediaFile(const QString& filePath, MediaType type) {
    if (isValidMediaFile(filePath)) {
        MediaItem item(filePath, type);
        rotationManager->addMediaItem(item);
        qDebug() << "Added media file:" << filePath;
    }
}

void MediaManager::removeMediaFile(const QString& filePath) {
    rotationManager->removeMediaItem(filePath);
}

void MediaManager::clearAllMedia() {
    rotationManager->clearMedia();
}

void MediaManager::setImageDuration(int seconds) {
    imageDuration = seconds;
    rotationManager->setDefaultImageDuration(seconds);
}

void MediaManager::setRotationMode(const QString& mode) {
    rotationMode = mode;
    rotationManager->setRotationMode(mode);
}

void MediaManager::setEffectsEnabled(bool enabled) {
    effectsEnabled = enabled;
}

void MediaManager::handleServerMediaCommand(const QJsonObject& command) {
    QString commandType = command["type"].toString();
    
    if (commandType == "add_media") {
        rotationManager->addServerMedia(command["data"].toObject());
    } else if (commandType == "load_playlist") {
        rotationManager->loadServerPlaylist(command["playlist"].toArray());
    } else if (commandType == "download_media") {
        QString url = command["url"].toString();
        QString filename = command["filename"].toString();
        downloadServerMedia(url, filename);
    } else if (commandType == "set_rotation_mode") {
        setRotationMode(command["mode"].toString());
    } else if (commandType == "play_effect") {
        QString effect = command["effect"].toString();
        int duration = command["duration"].toInt(3000);
        showEffect(effect, duration);
    }
}

void MediaManager::downloadServerMedia(const QString& url, const QString& filename) {
    QString localPath = mediaDirectory + "/" + filename;
    downloadManager->downloadMedia(url, localPath);
}

void MediaManager::loadSettings(const QJsonObject& settings) {
    QJsonObject mediaSettings = settings["MediaSettings"].toObject();
    
    imageDuration = mediaSettings["DefaultImageDuration"].toInt(300);
    rotationMode = mediaSettings["RotationMode"].toString("sequential");
    effectsEnabled = mediaSettings["EffectsEnabled"].toBool(true);
    
    QString imageDir = mediaSettings["ImageDirectory"].toString("media/images");
    QString videoDir = mediaSettings["VideoDirectory"].toString("media/videos");
    QString effectsDir = mediaSettings["EffectsDirectory"].toString("media/effects");
    
    // Load media from directories
    loadMediaFromDirectory(imageDir);
    loadMediaFromDirectory(videoDir);
    loadMediaFromDirectory(effectsDir);
    
    // Apply settings
    setImageDuration(imageDuration);
    setRotationMode(rotationMode);
    setEffectsEnabled(effectsEnabled);
}

void MediaManager::saveSettings() const {
    QSettings settings;
    settings.setValue("MediaManager/ImageDuration", imageDuration);
    settings.setValue("MediaManager/RotationMode", rotationMode);
    settings.setValue("MediaManager/EffectsEnabled", effectsEnabled);
    settings.setValue("MediaManager/MediaDirectory", mediaDirectory);
}

QStringList MediaManager::getSupportedVideoFormats() {
    return supportedVideoFormats;
}

QStringList MediaManager::getSupportedImageFormats() {
    return supportedImageFormats;
}

void MediaManager::onMediaRotationChanged(const MediaItem& item) {
    currentMediaItem = item;
    
    if (item.type == MediaType::Image) {
        displayImage(item.filePath);
        setCurrentIndex(mediaDisplayIndex);
    } else if (item.type == MediaType::Video) {
        displayVideo(item.filePath);
        setCurrentWidget(videoDisplayWidget);
    }
}

void MediaManager::onEffectFinished(const QString& effectName) {
    emit effectFinished(effectName);
    returnToGameDisplay();
}

void MediaManager::onMediaDownloaded(const QString& url, const QString& localPath) {
    Q_UNUSED(url)
    
    // Add the downloaded media to rotation
    MediaType type = getMediaType(localPath);
    addMediaFile(localPath, type);
    
    QString filename = QFileInfo(localPath).fileName();
    emit mediaDownloaded(filename);
}

void MediaManager::onVideoStateChanged(QMediaPlayer::State state) {
    if (state == QMediaPlayer::StoppedState) {
        // Video finished, continue rotation
        rotationManager->selectNextMedia();
    }
}

void MediaManager::onVideoError(QMediaPlayer::Error error) {
    QString errorString = mediaPlayer->errorString();
    qWarning() << "Video playback error:" << error << errorString;
    emit mediaError("Video playback error: " + errorString);
    
    // Continue rotation despite error
    rotationManager->selectNextMedia();
}

void MediaManager::displayImage(const QString& filePath) {
    QPixmap pixmap(filePath);
    if (!pixmap.isNull()) {
        imageDisplayLabel->setPixmap(pixmap.scaled(
            imageDisplayLabel->size(), 
            Qt::KeepAspectRatio, 
            Qt::SmoothTransformation
        ));
    } else {
        imageDisplayLabel->setText("Failed to load image");
        emit mediaError("Failed to load image: " + filePath);
    }
}

void MediaManager::displayVideo(const QString& filePath) {
    mediaPlayer->setVideoOutput(videoDisplayWidget);
    mediaPlayer->setMedia(QUrl::fromLocalFile(filePath));
    mediaPlayer->play();
}

void MediaManager::returnToGameDisplay() {
    if (gameDisplayIndex >= 0) {
        setCurrentIndex(gameDisplayIndex);
    }
}

QString MediaManager::getMediaDirectory() const {
    return QStandardPaths::writableLocation(QStandardPaths::DataLocation) + "/media";
}

void MediaManager::scanMediaDirectory() {
    QDir dir(mediaDirectory);
    if (!dir.exists()) {
        dir.mkpath(".");
        return;
    }
    
    // Scan for supported media files
    QStringList filters;
    for (const QString& format : supportedImageFormats) {
        filters << "*." + format;
    }
    for (const QString& format : supportedVideoFormats) {
        filters << "*." + format;
    }
    
    QFileInfoList files = dir.entryInfoList(filters, QDir::Files);
    for (const QFileInfo& fileInfo : files) {
        MediaType type = getMediaType(fileInfo.absoluteFilePath());
        addMediaFile(fileInfo.absoluteFilePath(), type);
    }
}

bool MediaManager::isValidMediaFile(const QString& filePath) const {
    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists()) return false;
    
    QString suffix = fileInfo.suffix().toLower();
    return supportedImageFormats.contains(suffix) || supportedVideoFormats.contains(suffix);
}

MediaType MediaManager::getMediaType(const QString& filePath) const {
    QString suffix = QFileInfo(filePath).suffix().toLower();
    
    if (supportedVideoFormats.contains(suffix)) {
        return MediaType::Video;
    } else if (supportedImageFormats.contains(suffix)) {
        return MediaType::Image;
    }
    
    return MediaType::Image; // Default
}