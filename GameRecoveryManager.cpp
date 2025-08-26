// GameRecoveryManager.cpp

#include <QWidget>
#include <QFrame>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QScrollArea>
#include <QPushButton>
#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>
#include <QTimer>
#include <QFont>
#include <QPalette>
#include <QPixmap>
#include <QPainter>
#include <QStyleOption>
#include <QVector>
#include <QJsonObject>

// Game Recovery Implimentation
GameRecoveryManager::GameRecoveryManager(QObject* parent) 
    : QObject(parent), gameActive(false), gameNumber(0) {
    
    recoveryFilePath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/game_recovery.json";
    QDir().mkpath(QFileInfo(recoveryFilePath).path());
    
    recoveryTimer = new QTimer(this);
    recoveryTimer->setSingleShot(true);
    recoveryTimer->setInterval(300000); // 5 minutes
    connect(recoveryTimer, &QTimer::timeout, this, &GameRecoveryManager::onRecoveryTimeout);
    
    loadRecoveryState();
}

void GameRecoveryManager::markGameActive(int gameNumber, const QJsonObject& gameState) {
    this->gameNumber = gameNumber;
    this->gameActive = true;
    
    currentRecoveryData = QJsonObject{
        {"game_active", true},
        {"game_number", gameNumber},
        {"timestamp", QDateTime::currentDateTime().toString(Qt::ISODate)},
        {"game_state", gameState}
    };
    
    saveRecoveryState();
    qDebug() << "Marked game" << gameNumber << "as active for recovery";
}

void GameRecoveryManager::markGameInactive() {
    gameActive = false;
    gameNumber = 0;
    
    currentRecoveryData = QJsonObject{
        {"game_active", false},
        {"game_number", 0},
        {"timestamp", QDateTime::currentDateTime().toString(Qt::ISODate)}
    };
    
    saveRecoveryState();
    qDebug() << "Marked game as inactive";
}

void GameRecoveryManager::checkForRecovery(QWidget* parent) {
    if (hasActiveGame()) {
        qDebug() << "Active game found, showing recovery dialog";
        showRecoveryDialog(parent);
    }
}

void GameRecoveryManager::showRecoveryDialog(QWidget* parent) {
    QDialog* dialog = new QDialog(parent);
    dialog->setWindowTitle("Game Recovery");
    dialog->setModal(true);
    dialog->setMinimumSize(400, 200);
    
    QVBoxLayout* layout = new QVBoxLayout(dialog);
    
    QLabel* messageLabel = new QLabel("A previous game was detected. Would you like to restore it?", dialog);
    messageLabel->setWordWrap(true);
    messageLabel->setFont(QFont("Arial", 12));
    
    QLabel* detailLabel = new QLabel(QString("Game #%1 from %2")
        .arg(currentRecoveryData["game_number"].toInt())
        .arg(currentRecoveryData["timestamp"].toString()), dialog);
    detailLabel->setStyleSheet("QLabel { color: gray; font-size: 10px; }");
    
    QLabel* timerLabel = new QLabel("Auto-decline in: 5:00", dialog);
    timerLabel->setAlignment(Qt::AlignCenter);
    timerLabel->setStyleSheet("QLabel { color: red; font-weight: bold; }");
    
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    QPushButton* restoreButton = new QPushButton("Restore Game", dialog);
    QPushButton* declineButton = new QPushButton("Start Fresh", dialog);
    
    restoreButton->setStyleSheet("QPushButton { background-color: green; color: white; font-size: 14px; padding: 10px; }");
    declineButton->setStyleSheet("QPushButton { background-color: red; color: white; font-size: 14px; padding: 10px; }");
    
    buttonLayout->addWidget(restoreButton);
    buttonLayout->addWidget(declineButton);
    
    layout->addWidget(messageLabel);
    layout->addWidget(detailLabel);
    layout->addStretch();
    layout->addWidget(timerLabel);
    layout->addLayout(buttonLayout);
    
    // Update timer display
    QTimer* displayTimer = new QTimer(dialog);
    int timeLeft = 300; // 5 minutes in seconds
    connect(displayTimer, &QTimer::timeout, [timerLabel, &timeLeft, dialog]() {
        timeLeft--;
        int minutes = timeLeft / 60;
        int seconds = timeLeft % 60;
        timerLabel->setText(QString("Auto-decline in: %1:%2")
                           .arg(minutes).arg(seconds, 2, 10, QChar('0')));
        if (timeLeft <= 0) {
            dialog->reject();
        }
    });
    displayTimer->start(1000);
    
    connect(restoreButton, &QPushButton::clicked, dialog, &QDialog::accept);
    connect(declineButton, &QPushButton::clicked, dialog, &QDialog::reject);
    
    // Start the auto-decline timer
    recoveryTimer->start();
    connect(recoveryTimer, &QTimer::timeout, dialog, &QDialog::reject);
    
    if (dialog->exec() == QDialog::Accepted) {
        recoveryTimer->stop();
        emit recoveryRequested(currentRecoveryData["game_state"].toObject());
    } else {
        recoveryTimer->stop();
        markGameInactive(); // Clear recovery state
        emit recoveryDeclined();
    }
    
    dialog->deleteLater();
}

void GameRecoveryManager::saveRecoveryState() {
    QFile file(recoveryFilePath);
    if (file.open(QIODevice::WriteOnly)) {
        QJsonDocument doc(currentRecoveryData);
        file.write(doc.toJson());
        file.close();
    }
}

void GameRecoveryManager::loadRecoveryState() {
    QFile file(recoveryFilePath);
    if (file.open(QIODevice::ReadOnly)) {
        QByteArray data = file.readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        currentRecoveryData = doc.object();
        
        gameActive = currentRecoveryData["game_active"].toBool();
        gameNumber = currentRecoveryData["game_number"].toInt();
        
        file.close();
    }

}
