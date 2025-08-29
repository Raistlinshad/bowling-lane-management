#include "MediaManager.h"
#include <QDebug>
#include <QApplication>
#include <QStandardPaths>
#include <QImageReader>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QTimer>

MediaManager::MediaManager(QWidget* parent) 
    : QStackedWidget(parent), isGameMode(false),
      stubDisplayWidget(nullptr), stubLabel(nullptr), 
      gameDisplayWidget(nullptr), videoDisplayWidget(nullptr),
      mediaPlayer(nullptr), effectTimer(nullptr) {
    setupUI();
}

void MediaManager::setupUI() {
    // Create stub display
    createStubDisplay();
    
    // Initialize timers
    effectTimer = new QTimer(this);
    effectTimer->setSingleShot(true);
    connect(effectTimer, &QTimer::timeout, this, &MediaManager::onEffectTimer);
    
    // Create multimedia components (stubs in non-multimedia build)
    videoDisplayWidget = new QVideoWidget(this);
    addWidget(videoDisplayWidget);
    
    mediaPlayer = new QMediaPlayer(this);
    connect(mediaPlayer, &QMediaPlayer::stateChanged, this, &MediaManager::onVideoStateChanged);
    connect(mediaPlayer, &QMediaPlayer::errorOccurred, this, &MediaManager::onVideoError);
    
    // Start with stub display
    setCurrentWidget(stubDisplayWidget);
}

void MediaManager::createStubDisplay() {
    stubDisplayWidget = new QWidget(this);
    stubDisplayWidget->setStyleSheet("background-color: #2b2b2b;");
    
    QVBoxLayout* layout = new QVBoxLayout(stubDisplayWidget);
    
    stubLabel = new QLabel("Bowling Lane Display\nWaiting for game...", stubDisplayWidget);
    stubLabel->setStyleSheet("color: white; font-size: 24px; font-weight: bold;");
    stubLabel->setAlignment(Qt::AlignCenter);
    
    layout->addWidget(stubLabel);
    addWidget(stubDisplayWidget);
}

void MediaManager::showGameDisplay(QWidget* gameWidget) {
    if (!gameWidget) return;
    
    gameDisplayWidget = gameWidget;
    addWidget(gameDisplayWidget);
    setCurrentWidget(gameDisplayWidget);
    isGameMode = true;
    
    qDebug() << "MediaManager: Switched to game display mode";
}

void MediaManager::showEffect(const QString& effect, int duration) {
    currentEffect = effect;
    
    // Update display for effect
    if (effect == "strike") {
        stubLabel->setText("STRIKE!");
        stubLabel->setStyleSheet("color: gold; font-size: 36px; font-weight: bold;");
    } else if (effect == "spare") {
        stubLabel->setText("SPARE!");
        stubLabel->setStyleSheet("color: lime; font-size: 36px; font-weight: bold;");
    } else {
        stubLabel->setText(effect.toUpper());
        stubLabel->setStyleSheet("color: cyan; font-size: 30px; font-weight: bold;");
    }
    
    if (!isGameMode) {
        setCurrentWidget(stubDisplayWidget);
    }
    
    emit effectStarted(effect);
    
    effectTimer->start(duration);
    qDebug() << "MediaManager: Showing effect" << effect << "for" << duration << "ms";
}

void MediaManager::showMediaRotation() {
    isGameMode = false;
    stubLabel->setText("Bowling Lane Display\nWaiting for game...");
    stubLabel->setStyleSheet("color: white; font-size: 24px; font-weight: bold;");
    setCurrentWidget(stubDisplayWidget);
    
    qDebug() << "MediaManager: Switched to media rotation mode";
}

void MediaManager::loadSettings(const QJsonObject& settings) {
    this->settings = settings;
    qDebug() << "MediaManager: Settings loaded";
}

void MediaManager::onEffectTimer() {
    emit effectFinished(currentEffect);
    
    if (isGameMode && gameDisplayWidget) {
        setCurrentWidget(gameDisplayWidget);
    } else {
        showMediaRotation();
    }
}

void MediaManager::onVideoStateChanged(QMediaPlayer::State state) {
    // Stub implementation
    Q_UNUSED(state)
    qDebug() << "MediaManager: Video state changed (stub)";
}

void MediaManager::onVideoError(QMediaPlayer::Error error) {
    // Stub implementation  
    Q_UNUSED(error)
    qDebug() << "MediaManager: Video error (stub)";
}