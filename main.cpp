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
    BowlingMainWindow(QWidget* parent = nullptr) : QMainWindow(parent), 
        gameActive(false), currentGameNumber(1) {
        setupUI();
        setupClient();
        setupGame();
        applyDarkTheme();
        loadGameColors();
    
        qDebug() << "=== CONSTRUCTOR COMPLETE ===";
    }
    

private slots:
    void onGameUpdated() {
        updateGameDisplay();
    }
    
    void onSpecialEffect(const QString& effect) {
        mediaDisplay->showEffect(effect);
    }
    
    void onHoldClicked() {
        if (gameActive) {
            game->holdGame();
        }
    }
    
    void onSkipClicked() {
        if (gameActive) {
            game->skipPlayer();
        }
    }
    
    void onResetClicked() {
        if (gameActive) {
            game->resetGame();
        }
    }
    
    void onCurrentPlayerChanged(const QString& playerName) {
        updateGameStatus();
    }
    
    void onGameStarted() {
        qDebug() << "=== GAME STARTED SIGNAL RECEIVED ===";
        qDebug() << "Current gameActive state:" << gameActive;
    
        gameActive = true;
        qDebug() << "Set gameActive to true";
    
        qDebug() << "About to call showGameInterface()";
        showGameInterface();
        qDebug() << "Finished showGameInterface()";
    
        qDebug() << "About to call applyGameColors()";
        applyGameColors();
        qDebug() << "Finished applyGameColors()";
    
        qDebug() << "=== GAME STARTED PROCESSING COMPLETE ===";
    }
    
    void onGameEnded(const QJsonObject& results) {
        Q_UNUSED(results)
        gameActive = false;
        currentGameNumber++;
        hideGameInterface();
        mediaDisplay->showMediaRotation(); // Return to media rotation
    }
    
    void onGameCommand(const QString& type, const QJsonObject& data) {
        qDebug() << "=== RECEIVED GAME COMMAND ===" << type;
        qDebug() << "Current gameActive state:" << gameActive;
    
        if (type == "quick_game") {
            // End current game if one is active
            if (gameActive) {
                qDebug() << "Ending current game to start new one";
                game->endGame();
                // Note: endGame() will call onGameEnded() which sets gameActive = false
            }
        
            qDebug() << "Starting new quick game";
            currentGameType = "quick_game";
            game->startGame(data);
        
        } else if (type == "league_game") {
            if (gameActive) {
                qDebug() << "Ending current game to start new league game";
                game->endGame();
            }
        
            qDebug() << "Starting new league game";
            currentGameType = "league_game";
            game->startGame(data);
        
        } else if (type == "tournament_game") {
            if (gameActive) {
                qDebug() << "Ending current game to start new tournament game";
                game->endGame();
            }
        
            qDebug() << "Starting new tournament game";
            currentGameType = "tournament_game";
            game->startGame(data);
        
        } else if (type == "status_update") {
            sendGameStatus();
        } else if (type == "player_update_add") {
            QString playerName = data["player_name"].toString();
            if (gameActive) game->addPlayer(playerName);
        } else if (type == "player_update_remove") {
            QString playerName = data["player_name"].toString();
            if (gameActive) game->removePlayer(playerName);
        } else if (type == "score_update") {
            if (gameActive) game->updateScore(data);
        } else if (type == "hold_update") {
            bool hold = data["hold"].toBool();
            if (gameActive && hold != game->isGameHeld()) {
                game->holdGame();
            }
        } else if (type == "move_to") {
            handleMoveToLane(data);
        } else if (type == "scroll_update") {
            updateScrollText(data["text"].toString());
        } else {
            qDebug() << "Unknown game command type:" << type;
        }
    
        qDebug() << "=== FINISHED PROCESSING GAME COMMAND ===";
    }
private:
    void setupUI() {
        setWindowTitle("Canadian 5-Pin Bowling");
        setMinimumSize(1200, 800);
        
        QWidget* centralWidget = new QWidget(this);
        setCentralWidget(centralWidget);
        
        QVBoxLayout* mainLayout = new QVBoxLayout(centralWidget);
        
        // Media display area (full screen when no game)
        mediaDisplay = new MediaManager(this);
        mediaDisplay->setMinimumHeight(600);
        
        // Game interface container (hidden by default)
        gameInterfaceWidget = new QWidget(this);
        gameInterfaceWidget->hide();
        setupGameInterface();
        
        // Layout assembly
        mainLayout->addWidget(mediaDisplay, 1);
        mainLayout->addWidget(gameInterfaceWidget, 0);
        
        // Start with media display only
        mediaDisplay->showMediaRotation();
    }
    
    void setupGameInterface() {
        QVBoxLayout* gameLayout = new QVBoxLayout(gameInterfaceWidget);
        
        // Game display area
        gameDisplayArea = new QScrollArea(this);
        gameDisplayArea->setWidgetResizable(true);
        gameDisplayArea->setMinimumHeight(250);
        gameDisplayArea->setMaximumHeight(350);
        
        gameWidget = new QWidget();
        gameWidgetLayout = new QVBoxLayout(gameWidget);
        gameDisplayArea->setWidget(gameWidget);
        
        // Game status area (includes pin display)
        gameStatus = new GameStatusWidget(this);
        
        // Control buttons area
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
        
        // Assemble game interface
        gameLayout->addWidget(gameDisplayArea, 1);
        gameLayout->addWidget(gameStatus, 0);
        gameLayout->addLayout(buttonLayout, 0);
    }
    
    void showGameInterface() {
        qDebug() << "=== SHOWING GAME INTERFACE ===";
    
        qDebug() << "Current mediaDisplay max height:" << mediaDisplay->maximumHeight();
        qDebug() << "Current gameInterfaceWidget visibility:" << gameInterfaceWidget->isVisible();
    
        // Shrink media display to make room for game interface
        mediaDisplay->setMaximumHeight(400);
        qDebug() << "Set mediaDisplay max height to 400";
    
        gameInterfaceWidget->show();
        qDebug() << "Called gameInterfaceWidget->show(), now visible:" << gameInterfaceWidget->isVisible();
    
        // Show the game display in media area
        qDebug() << "About to call mediaDisplay->showGameDisplay()";
        mediaDisplay->showGameDisplay(gameDisplayArea);
        qDebug() << "Finished mediaDisplay->showGameDisplay()";
    
        qDebug() << "=== GAME INTERFACE DISPLAY COMPLETE ===";
    }
    
    void hideGameInterface() {
        // Hide game interface and expand media display
        gameInterfaceWidget->hide();
        mediaDisplay->setMaximumHeight(QWIDGETSIZE_MAX);
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
        qDebug() << "=== SETTING UP GAME ===";
    
        game = new QuickGame(this);
        qDebug() << "Created QuickGame instance";
    
        // Connect with debug verification
        bool connected = false;
    
        connected = connect(game, &QuickGame::gameUpdated, this, &BowlingMainWindow::onGameUpdated);
        qDebug() << "Connected gameUpdated signal:" << connected;
    
        connected = connect(game, &QuickGame::specialEffect, this, &BowlingMainWindow::onSpecialEffect);
        qDebug() << "Connected specialEffect signal:" << connected;
    
        connected = connect(game, &QuickGame::currentPlayerChanged, this, &BowlingMainWindow::onCurrentPlayerChanged);
        qDebug() << "Connected currentPlayerChanged signal:" << connected;
    
        connected = connect(game, &QuickGame::gameStarted, this, &BowlingMainWindow::onGameStarted);
        qDebug() << "Connected gameStarted signal:" << connected << "*** THIS IS CRITICAL ***";
    
        connected = connect(game, &QuickGame::gameEnded, this, &BowlingMainWindow::onGameEnded);
        qDebug() << "Connected gameEnded signal:" << connected;
    
        connected = connect(game, &QuickGame::gameHeld, this, [this](bool held) {
            qDebug() << "Game hold state changed to:" << held;
            holdButton->setText(held ? "RESUME" : "HOLD");
            holdButton->setStyleSheet(held ? 
                "QPushButton { background-color: red; color: white; font-size: 18px; font-weight: bold; }" :
                "QPushButton { background-color: green; color: white; font-size: 18px; font-weight: bold; }");
        });
        qDebug() << "Connected gameHeld signal:" << connected;
    
        qDebug() << "=== GAME SETUP COMPLETE ===";
    }
    
    void applyDarkTheme() {
        // Apply dark theme to entire application
        QString darkStyle = R"(
            QMainWindow {
                background-color: #2b2b2b;
                color: #ffffff;
            }
            QWidget {
                background-color: #2b2b2b;
                color: #ffffff;
            }
            QFrame {
                background-color: #3c3c3c;
                border: 1px solid #555555;
            }
            QLabel {
                background-color: transparent;
                color: #ffffff;
            }
            QScrollArea {
                background-color: #2b2b2b;
                border: 1px solid #555555;
            }
            QPushButton {
                background-color: #4a4a4a;
                border: 2px solid #666666;
                padding: 5px;
                color: #ffffff;
                font-weight: bold;
            }
            QPushButton:hover {
                background-color: #5a5a5a;
                border-color: #777777;
            }
            QPushButton:pressed {
                background-color: #3a3a3a;
            }
        )";
        
        setStyleSheet(darkStyle);
    }
    
    void loadGameColors() {
        QSettings settings("settings.ini", QSettings::IniFormat);
        settings.beginGroup("GameColors");
        
        gameColors.clear();
        for (int i = 1; i <= 6; ++i) {
            QString bgKey = QString("Game%1_Background").arg(i);
            QString fgKey = QString("Game%1_Foreground").arg(i);
            
            ColorScheme scheme;
            scheme.background = settings.value(bgKey, "blue").toString();
            scheme.foreground = settings.value(fgKey, "white").toString();
            gameColors.append(scheme);
        }
        qDebug() << "=== CONSTRUCTOR COMPLETE - Adding manual test timer ===";
        settings.endGroup();
    }
    
    void applyGameColors() {
        if (gameColors.isEmpty()) return;
        
        // Use modulo to cycle through colors
        int colorIndex = (currentGameNumber - 1) % gameColors.size();
        const ColorScheme& scheme = gameColors[colorIndex];
        
        // Apply colors to game interface elements
        QString gameStyle = QString(R"(
            #gameInterfaceWidget {
                background-color: %1;
                color: %2;
            }
            #gameInterfaceWidget QLabel {
                background-color: transparent;
                color: %2;
            }
            #gameInterfaceWidget QFrame {
                background-color: %1;
                color: %2;
                border: 2px solid %2;
            }
        )").arg(scheme.background, scheme.foreground);
        
        gameInterfaceWidget->setObjectName("gameInterfaceWidget");
        gameInterfaceWidget->setStyleSheet(gameStyle);
        
        // Update game status widget colors
        gameStatus->setGameStyleSheet(scheme.background, scheme.foreground);
    }
    
    void sendGameStatus() {
        if (!gameActive || !game) return;
        
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
        if (!gameActive) return;
        
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
        if (!gameActive || !game) return;
        
        // Clear existing bowler widgets
        QLayoutItem* item;
        while ((item = gameWidgetLayout->takeAt(0)) != nullptr) {
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
            gameWidgetLayout->addWidget(bowlerWidget);
        }
        
        gameWidgetLayout->addStretch();
        updateGameStatus();
    }
    
    void updateGameStatus() {
        if (!gameActive || !game || game->getBowlers().isEmpty()) {
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

    // Data members
    struct ColorScheme {
        QString background;
        QString foreground;
    };
    
    // UI Components
    MediaManager* mediaDisplay;
    QWidget* gameInterfaceWidget;
    QScrollArea* gameDisplayArea;
    QWidget* gameWidget;
    QVBoxLayout* gameWidgetLayout;
    GameStatusWidget* gameStatus;
    
    QPushButton* holdButton;
    QPushButton* skipButton;
    QPushButton* resetButton;
    QLabel* scrollLabel = nullptr;
    
    LaneClient* client;
    QuickGame* game;
    
    // Game state
    bool gameActive;
    QString currentGameType;
    int currentGameNumber;
    QVector<ColorScheme> gameColors;
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