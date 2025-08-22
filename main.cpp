#include <QApplication>
#include <QMainWindow>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QFrame>
#include <QScrollArea>
#include <QTimer>
#include <QStackedWidget>
#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>
#ifdef MULTIMEDIA_SUPPORT
#include <QMediaPlayer>
#include <QVideoWidget>
#endif
#include <QPixmap>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QSettings>
#include <QFile>
#include <QDir>
#include <QRandomGenerator>
#include <QDebug>
#include <QProcess>
#include "LaneClient.h"
#include "QuickGame.h"
#include "BowlingWidgets.h"
#include "MediaManager.h"

// Main bowling window
class BowlingMainWindow : public QMainWindow {
    Q_OBJECT
    
public:
    BowlingMainWindow(QWidget* parent = nullptr) : QMainWindow(parent) {
        setupUI();
        setupClient();
        setupGame();
    }

private slots:
    void onGameUpdated() {
        updateGameDisplay();
    }
    
    void onSpecialEffect(const QString& effect) {
        mediaDisplay->showEffect(effect);
    }
    
    void onHoldClicked() {
        game->holdGame();
    }
    
    void onSkipClicked() {
        game->skipPlayer();
    }
    
    void onResetClicked() {
        game->resetGame();
    }
    
    void onCurrentPlayerChanged(const QString& playerName) {
        updateGameStatus();
    }
    
    void onGameCommand(const QString& type, const QJsonObject& data) {
        qDebug() << "Received game command:" << type;
        
        if (type == "quick_game") {
            game->startGame(data);
        } else if (type == "status_update") {
            sendGameStatus();
        } else if (type == "player_update_add") {
            QString playerName = data["player_name"].toString();
            game->addPlayer(playerName);
        } else if (type == "player_update_remove") {
            QString playerName = data["player_name"].toString();
            game->removePlayer(playerName);
        } else if (type == "score_update") {
            game->updateScore(data);
        } else if (type == "hold_update") {
            bool hold = data["hold"].toBool();
            if (hold != game->isGameHeld()) {
                game->holdGame();
            }
        } else if (type == "move_to") {
            handleMoveToLane(data);
        } else if (type == "scroll_update") {
            updateScrollText(data["text"].toString());
        }
    }

private:
    void setupUI() {
        setWindowTitle("Canadian 5-Pin Bowling");
        setMinimumSize(1200, 800);
        
        QWidget* centralWidget = new QWidget(this);
        setCentralWidget(centralWidget);
        
        QVBoxLayout* mainLayout = new QVBoxLayout(centralWidget);
        
        // Media display area (blue background)
        mediaDisplay = new MediaManager(this);
        mediaDisplay->setMinimumHeight(400);
        
        // Game display area
        gameDisplayArea = new QScrollArea(this);
        gameDisplayArea->setWidgetResizable(true);
        gameDisplayArea->setMinimumHeight(300);
        
        gameWidget = new QWidget();
        gameLayout = new QVBoxLayout(gameWidget);
        gameDisplayArea->setWidget(gameWidget);
        
        // Set game widget in media display
        mediaDisplay->showGameDisplay(gameDisplayArea);
        
        // Game status area
        gameStatus = new GameStatusWidget(this);
        
        // Button area
        QHBoxLayout* buttonLayout = new QHBoxLayout();
        
        holdButton = new QPushButton("HOLD", this);
        skipButton = new QPushButton("SKIP", this);
        resetButton = new QPushButton("RESET", this);
        
        holdButton->setMinimumHeight(60);
        skipButton->setMinimumHeight(60);
        resetButton->setMinimumHeight(60);
        
        holdButton->setStyleSheet("QPushButton { font-size: 18px; font-weight: bold; }");
        skipButton->setStyleSheet("QPushButton { font-size: 18px; font-weight: bold; }");
        resetButton->setStyleSheet("QPushButton { font-size: 18px; font-weight: bold; }");
        
        connect(holdButton, &QPushButton::clicked, this, &BowlingMainWindow::onHoldClicked);
        connect(skipButton, &QPushButton::clicked, this, &BowlingMainWindow::onSkipClicked);
        connect(resetButton, &QPushButton::clicked, this, &BowlingMainWindow::onResetClicked);
        
        buttonLayout->addWidget(holdButton);
        buttonLayout->addWidget(skipButton);
        buttonLayout->addWidget(resetButton);
        buttonLayout->addStretch();
        
        // Layout assembly
        mainLayout->addWidget(mediaDisplay, 2);
        mainLayout->addWidget(gameStatus, 0);
        mainLayout->addLayout(buttonLayout, 0);
    }
    
    void setupClient() {
        // Load settings
        QSettings settings("settings.ini", QSettings::IniFormat);
        int laneId = settings.value("Lane/id", 1).toInt();
        QString serverHost = settings.value("Server/host", "192.168.2.243").toString();
        int serverPort = settings.value("Server/port", 50005).toInt();
        
        client = new LaneClient(laneId, this);
        client->setServerAddress(serverHost, serverPort);
        
        connect(client, &LaneClient::gameCommandReceived, this, &BowlingMainWindow::onGameCommand);
        
        client->start();
    }
    
    void setupGame() {
        game = new QuickGame(this);
        
        connect(game, &QuickGame::gameUpdated, this, &BowlingMainWindow::onGameUpdated);
        connect(game, &QuickGame::specialEffect, this, &BowlingMainWindow::onSpecialEffect);
        connect(game, &QuickGame::currentPlayerChanged, this, &BowlingMainWindow::onCurrentPlayerChanged);
        connect(game, &QuickGame::gameHeld, this, [this](bool held) {
            holdButton->setText(held ? "RESUME" : "HOLD");
            holdButton->setStyleSheet(held ? 
                "QPushButton { background-color: red; color: white; font-size: 18px; font-weight: bold; }" :
                "QPushButton { background-color: green; color: white; font-size: 18px; font-weight: bold; }");
        });
    }
    
    void sendGameStatus() {
        QJsonObject status;
        status["type"] = "game_status";
        status["lane_id"] = client->getLaneId();
        status["current_player"] = game->getCurrentBowler().name;
        status["game_held"] = game->isGameHeld();
        status["frame"] = game->getCurrentBowler().currentFrame + 1;
        status["ball"] = game->getCurrentBowler().getCurrentFrame().balls.size() + 1;
        
        QJsonArray bowlersArray;
        for (const Bowler& bowler : game->getBowlers()) {
            QJsonObject bowlerObj;
            bowlerObj["name"] = bowler.name;
            bowlerObj["total_score"] = bowler.totalScore;
            bowlerObj["current_frame"] = bowler.currentFrame + 1;
            bowlersArray.append(bowlerObj);
        }
        status["bowlers"] = bowlersArray;
        
        client->sendMessage(status);
    }
    
    void handleMoveToLane(const QJsonObject& data) {
        QJsonObject gameState;
        gameState["bowlers"] = serializeBowlers();
        gameState["current_bowler"] = game->getCurrentBowlerIndex();
        gameState["held"] = game->isGameHeld();
        
        QJsonObject moveMessage;
        moveMessage["type"] = "game_state_transfer";
        moveMessage["target_lane"] = data["target_lane"];
        moveMessage["game_data"] = gameState;
        
        client->sendMessage(moveMessage);
        game->resetGame();
    }
    
    QJsonArray serializeBowlers() {
        QJsonArray bowlersArray;
        for (const Bowler& bowler : game->getBowlers()) {
            bowlersArray.append(bowler.toJson());
        }
        return bowlersArray;
    }
    
    void updateScrollText(const QString& text) {
        if (!scrollLabel) {
            scrollLabel = new QLabel(this);
            scrollLabel->setStyleSheet("QLabel { background-color: black; color: yellow; font-size: 16px; padding: 5px; }");
            static_cast<QVBoxLayout*>(centralWidget()->layout())->addWidget(scrollLabel);
        }
        scrollLabel->setText(text);
    }
    
    void updateGameDisplay() {
        // Clear existing bowler widgets
        QLayoutItem* item;
        while ((item = gameLayout->takeAt(0)) != nullptr) {
            delete item->widget();
            delete item;
        }
        
        const QVector<Bowler>& bowlers = game->getBowlers();
        int currentIdx = game->getCurrentBowlerIndex();
        
        // Display up to 6 bowlers, prioritizing current player
        QVector<int> displayOrder;
        
        if (!bowlers.isEmpty()) {
            displayOrder.append(currentIdx);
        }
        
        for (int i = 0; i < bowlers.size() && displayOrder.size() < 6; ++i) {
            if (i != currentIdx) {
                displayOrder.append(i);
            }
        }
        
        // Create bowler widgets
        for (int i = 0; i < displayOrder.size(); ++i) {
            int bowlerIdx = displayOrder[i];
            bool isCurrent = (bowlerIdx == currentIdx);
            
            BowlerWidget* bowlerWidget = new BowlerWidget(bowlers[bowlerIdx], isCurrent);
            gameLayout->addWidget(bowlerWidget);
        }
        
        gameLayout->addStretch();
        updateGameStatus();
    }
    
    void updateGameStatus() {
        if (game->getBowlers().isEmpty()) {
            gameStatus->resetStatus();
            return;
        }
        
        const Bowler& currentBowler = game->getCurrentBowler();
        const Frame& currentFrame = currentBowler.getCurrentFrame();
        
        QVector<int> pinStates = game->getCurrentPinStates();
        
        gameStatus->updateStatus(
            currentBowler.name,
            currentBowler.currentFrame,
            currentFrame.balls.size(),
            pinStates
        );
    }

    // UI Components
    MediaManager* mediaDisplay;
    QScrollArea* gameDisplayArea;
    QWidget* gameWidget;
    QVBoxLayout* gameLayout;
    GameStatusWidget* gameStatus;
    
    QPushButton* holdButton;
    QPushButton* skipButton;
    QPushButton* resetButton;
    QLabel* scrollLabel = nullptr;
    
    LaneClient* client;
    QuickGame* game;
};

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    
    app.setApplicationName("Canadian5PinBowling");
    app.setApplicationVersion("1.0");
    app.setOrganizationName("BowlingCenter");
    
    BowlingMainWindow window;
    window.show();
    
    return app.exec();
}

#include "main.moc"
