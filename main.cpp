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
#include "QuickGameDialog.h"
#include "QuickStartDialog.h"
#include "BowlingWidgets.h"
#include "MediaManager.h"
#include "ThreeSixNineTracker.h"
#include "GameStatistics.h"
#include "GameRecoveryManager.h"

// Main bowling window
class BowlingMainWindow : public QMainWindow {
    Q_OBJECT
    
public:
    BowlingMainWindow(QWidget* parent = nullptr) : QMainWindow(parent), 
        gameActive(false), currentGameNumber(1) {
        
        // Initialize systems
        gameRecovery = new GameRecoveryManager(this);
        gameStatistics = new GameStatistics(this);
        gameStatus = new GameStatusWidget(this);
        threeSixNine = new ThreeSixNineTracker(this);
        
        setupUI();
        setupClient();
        setupGame();
        applyDarkTheme();
        loadGameColors();
        
        // Connect recovery system
        connect(gameRecovery, &GameRecoveryManager::recoveryRequested, 
                this, &BowlingMainWindow::onGameRecoveryRequested);
        connect(gameRecovery, &GameRecoveryManager::recoveryDeclined,
                this, &BowlingMainWindow::onGameRecoveryDeclined);
        
        // Connect statistics system
        connect(gameStatistics, &GameStatistics::newHighScore,
                this, &BowlingMainWindow::onNewHighScore);
        connect(gameStatistics, &GameStatistics::newStrikeRecord,
                this, &BowlingMainWindow::onNewStrikeRecord);
        
        // Connect 3-6-9 system
        connect(threeSixNine, &ThreeSixNineTracker::participantWon,
                this, &BowlingMainWindow::onThreeSixNineWin);
        connect(threeSixNine, &ThreeSixNineTracker::participantAlmostWon,
                this, &BowlingMainWindow::onThreeSixNineAlmostWin);
        
        qDebug() << "=== CONSTRUCTOR COMPLETE ===";
        
        // Check for game recovery on startup
        QTimer::singleShot(1000, this, [this]() {
            gameRecovery->checkForRecovery(this);
        });
    }

private slots:
    void onGameUpdated() {
        updateGameDisplay();
        updateGameStatus();
        
        // Save game state for recovery
        if (gameActive && game) {
            QJsonObject gameState = game->getGameState();
            gameRecovery->markGameActive(currentGameNumber, gameState);
        }
    }
    
    void onSpecialEffect(const QString& effect) {
        mediaDisplay->showEffect(effect);
        
        // Update message display
        if (effect == "strike") {
            messageScrollArea->setText("STRIKE! Excellent bowling!");
        } else if (effect == "spare") {
            messageScrollArea->setText("SPARE! Nice pickup!");
        }
    }
    
    void onGameStarted() {
        qDebug() << "=== GAME STARTED ===";
        gameActive = true;
        
        showGameInterface();
        applyGameColors();
        
        // Initialize 3-6-9 if enabled in game data
        if (currentGameData.contains("display_options")) {
            QJsonObject displayOpts = currentGameData["display_options"].toObject();
            if (displayOpts.contains("three_six_nine") && displayOpts["three_six_nine"].toObject()["enabled"].toBool()) {
                initializeThreeSixNine(displayOpts["three_six_nine"].toObject());
            }
        }
        
        // Mark game as active for recovery
        QJsonObject gameState = game->getGameState();
        gameRecovery->markGameActive(currentGameNumber, gameState);
    }
    
    void onGameEnded(const QJsonObject& results) {
        qDebug() << "=== GAME ENDED ===";
        
        // Record statistics
        if (game) {
            gameStatistics->recordGameCompletion(game->getBowlers(), currentGameType, currentGameNumber);
        }
        
        // Clear recovery state
        gameRecovery->markGameInactive();
        
        gameActive = false;
        currentGameNumber++;
        hideGameInterface();
        
        // Show completion message
        QString completionMsg = QString("Game %1 Complete! Thank you for playing.").arg(currentGameNumber - 1);
        messageScrollArea->setText(completionMsg);
        messageScrollArea->startScrolling();
        
        // Return to media rotation after delay
        QTimer::singleShot(5000, this, [this]() {
            mediaDisplay->showMediaRotation();
        });
    }
    
    void onBallProcessed(const QJsonObject& ballData) {
        QString bowlerName = ballData["bowler"].toString();
        int frame = ballData["frame"].toInt();
        int ballValue = ballData["value"].toInt();
        bool isStrike = (ballValue == 15);
        bool isSpare = ballData.contains("is_spare") ? ballData["is_spare"].toBool() : false;
        
        // Record for statistics
        if (game) {
            // Create ball object for statistics
            QJsonArray pinsArray = ballData["pins"].toArray();
            QVector<int> pins;
            for (const QJsonValue& val : pinsArray) {
                pins.append(val.toInt());
            }
            Ball ball(pins, ballValue);
            gameStatistics->recordBallThrown(bowlerName, frame, ball, isStrike, isSpare);
        }
        
        // Update 3-6-9 tracking
        if (threeSixNine->isActive()) {
            threeSixNine->recordFrameResult(bowlerName, currentGameNumber, frame, isStrike);
        }
        
        // Send to server
        client->sendMessage(ballData);

        updateGameStatus();
    }
    
    void onGameRecoveryRequested(const QJsonObject& gameState) {
        qDebug() << "Game recovery requested";
        
        // Restore the game state
        if (game) {
            game->loadGameState(gameState);
            onGameStarted(); // Show interface and activate game
        }
    }
    
    void onGameRecoveryDeclined() {
        qDebug() << "Game recovery declined";
        // Continue with normal startup
    }
    
    void onNewHighScore(const GameStatistics::HighScoreRecord& record) {
        QString message = QString("NEW HIGH SCORE! %1 scored %2 points!")
                         .arg(record.bowlerName).arg(record.score);
        messageScrollArea->setText(message);
        messageScrollArea->startScrolling();
        
        qDebug() << "New high score:" << message;
    }
    
    void onNewStrikeRecord(const GameStatistics::StrikeRecord& record) {
        QString message = QString("NEW STRIKE RECORD! %1 achieved %2 consecutive strikes!")
                         .arg(record.bowlerName).arg(record.consecutiveStrikes);
        messageScrollArea->setText(message);
        messageScrollArea->startScrolling();
        
        qDebug() << "New strike record:" << message;
    }
    
    void onThreeSixNineWin(const QString& bowlerName) {
        QString message = QString("3-6-9 WINNER! Congratulations %1!").arg(bowlerName);
        messageScrollArea->setText(message);
        messageScrollArea->startScrolling();
        updateGameDisplay(); // Refresh to show winner status
    }
    
    void onThreeSixNineAlmostWin(const QString& bowlerName) {
        QString message = QString("6 of 7! Great job %1!").arg(bowlerName);
        messageScrollArea->setText(message);
        messageScrollArea->startScrolling();
        updateGameDisplay(); // Refresh to show status
    }
    
    void onGameCommand(const QString& type, const QJsonObject& data) {
        qDebug() << "=== RECEIVED GAME COMMAND ===" << type;
        
        // Store current game data for reference
        currentGameData = data;
        
        if (type == "quick_game") {
            if (gameActive) {
                qDebug() << "Ending current game to start new quick game";
                game->endGame();
            }
            currentGameType = "quick_game";
            game->startGame(data);
            
        } else if (type == "league_game") {
            if (gameActive) {
                qDebug() << "Ending current game to start new league game";
                game->endGame();
            }
            currentGameType = "league_game";
            game->startGame(data);
            
        } else if (type == "display_mode_change") {
            handleDisplayModeChange(data);
            
        } else if (type == "team_move") {
            handleTeamMove(data);
            
        } else if (type == "scroll_message") {
            handleScrollMessage(data);
            
        } else if (type == "three_six_nine_toggle") {
            handleThreeSixNineToggle(data);
            
        } else {
            // Handle other commands...
            qDebug() << "Unhandled game command:" << type;
        }
    }

private:
    void setupGameInterface() {
        QVBoxLayout* gameLayout = new QVBoxLayout(gameInterfaceWidget);
        gameLayout->setContentsMargins(10, 10, 10, 10);
        gameLayout->setSpacing(5);
    
        // MAIN GAME AREA (takes most of screen space - optimized for 1920x1080)
        gameDisplayArea = new QScrollArea(this);
        gameDisplayArea->setWidgetResizable(true);
        gameDisplayArea->setMinimumHeight(800);
        gameDisplayArea->setStyleSheet("QScrollArea { border: 2px solid #555555; background-color: #2b2b2b; }");
    
        gameWidget = new QWidget();
        gameWidgetLayout = new QVBoxLayout(gameWidget);
        gameDisplayArea->setWidget(gameWidget);
    
        // BOTTOM CONTROL BAR (horizontal layout)
        QHBoxLayout* bottomBarLayout = new QHBoxLayout();
        bottomBarLayout->setSpacing(10);
    
        // Left: Control Buttons
        QHBoxLayout* buttonLayout = new QHBoxLayout();
        holdButton = new QPushButton("HOLD", this);
        skipButton = new QPushButton("SKIP", this);
        resetButton = new QPushButton("RESET", this);
    
        holdButton->setMinimumSize(120, 60);
        skipButton->setMinimumSize(120, 60);
        resetButton->setMinimumSize(120, 60);
    
        connect(holdButton, &QPushButton::clicked, this, &BowlingMainWindow::onHoldClicked);
        connect(skipButton, &QPushButton::clicked, this, &BowlingMainWindow::onSkipClicked);
        connect(resetButton, &QPushButton::clicked, this, &BowlingMainWindow::onResetClicked);
    
        buttonLayout->addWidget(holdButton);
        buttonLayout->addWidget(skipButton);
        buttonLayout->addWidget(resetButton);
    
        // Center: Scrolling Message Area
        messageScrollArea = new ScrollTextWidget(this);
        messageScrollArea->setText("Welcome to Canadian 5-Pin Bowling");
        messageScrollArea->setMinimumHeight(60);
        messageScrollArea->setStyleSheet("QLabel { background-color: black; color: yellow; font-size: 14px; border: 1px solid #555555; }");
    
        // Right: Pin Display
        pinDisplay = new PinDisplayWidget(this);
        pinDisplay->setDisplayMode("small");
        pinDisplay->setMinimumSize(140, 60);
        pinDisplay->setMaximumSize(180, 60);
    
        // Assemble bottom bar
        bottomBarLayout->addLayout(buttonLayout, 0);          // Fixed width buttons
        bottomBarLayout->addWidget(messageScrollArea, 1);     // Expandable message area
        bottomBarLayout->addWidget(pinDisplay, 0);            // Fixed width pin display
    
        // Assemble main layout
        gameLayout->addWidget(gameDisplayArea, 1);    // Takes most space
        gameLayout->addLayout(bottomBarLayout, 0);    // Fixed height bottom bar
    }
    
    void initializeThreeSixNine(const QJsonObject& config) {
        if (!config["enabled"].toBool()) return;
        
        QVector<QString> bowlerNames;
        for (const Bowler& bowler : game->getBowlers()) {
            bowlerNames.append(bowler.name);
        }
        
        // Parse target frames from config
        QJsonArray framesArray = config["frames"].toArray();
        QVector<int> targetFrames;
        for (const QJsonValue& value : framesArray) {
            targetFrames.append(value.toInt());
        }
        
        ThreeSixNineTracker::ParticipationMode mode = config["selectable"].toBool() ?
            ThreeSixNineTracker::ParticipationMode::Selectable :
            ThreeSixNineTracker::ParticipationMode::Everyone;
        
        threeSixNine->initialize(bowlerNames, targetFrames, mode);
        
        qDebug() << "3-6-9 game initialized with" << targetFrames.size() << "target frames";
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
        
        // Create enhanced bowler widgets with 3-6-9 integration
        for (int i = 0; i < bowlers.size(); ++i) {
            bool isCurrent = (i == currentIdx);
            
            // Prepare display options for this bowler
            QJsonObject displayOptions;
            if (currentGameData.contains("display_options")) {
                displayOptions = currentGameData["display_options"].toObject();
            }
            
            // Add 3-6-9 status if active
            if (threeSixNine->isActive()) {
                displayOptions["three_six_nine_status"] = threeSixNine->getStatusText(bowlers[i].name);
                displayOptions["three_six_nine_dots"] = threeSixNine->getDotsCount(bowlers[i].name);
            }
            
            EnhancedBowlerWidget* bowlerWidget = new EnhancedBowlerWidget(bowlers[i], isCurrent, displayOptions);
            
            // Apply current player highlighting
            if (isCurrent) {
                bowlerWidget->setStyleSheet("QFrame { border: 3px solid #FFD700; background-color: #3a3a3a; }");
            }
            
            gameWidgetLayout->addWidget(bowlerWidget);
        }
        
        gameWidgetLayout->addStretch();
        
        // Update pin display
        QVector<int> pinStates = game->getCurrentPinStates();
        pinDisplay->setPinStates(pinStates);
    }
    
    void handleDisplayModeChange(const QJsonObject& data) {
        QString frameMode = data["frame_mode"].toString();
        int frameStart = data["frame_start"].toInt();
        
        // Update display options and refresh
        currentGameData["display_options"] = data;
        updateGameDisplay();
        
        qDebug() << "Display mode changed to:" << frameMode << "starting at frame" << frameStart;
    }
    
    void handleTeamMove(const QJsonObject& data) {
        if (!gameActive) return;
        
        QString targetLane = data["target_lane"].toString();
        messageScrollArea->setText(QString("Team moving to Lane %1...").arg(targetLane));
        messageScrollArea->startScrolling();
        
        // Send current game state to target lane
        QJsonObject gameState = game->getGameState();
        QJsonObject moveMessage;
        moveMessage["type"] = "team_move_data";
        moveMessage["source_lane"] = client->getLaneId();
        moveMessage["target_lane"] = targetLane;
        moveMessage["game_state"] = gameState;
        
        client->sendMessage(moveMessage);
        
        // Reset local machine and show waiting message
        // game->sendMachineCommand("machine_reset", QJsonObject{{"immediate", true}});
        
        // Hide game interface and show waiting screen
        gameInterfaceWidget->hide();
        messageScrollArea->setText("Waiting for other team...");
        
        qDebug() << "Team move initiated to lane" << targetLane;
    }
    
    void handleScrollMessage(const QJsonObject& data) {
        QString text = data["text"].toString();
        int duration = 10000; // default
        if (data.contains("duration")) {
            duration = data["duration"].toInt();
        }
        
        messageScrollArea->setText(text);
        messageScrollArea->startScrolling();
        
        // Auto-clear after duration
        QTimer::singleShot(duration, this, [this]() {
            messageScrollArea->setText("Welcome to Canadian 5-Pin Bowling");
        });
    }
    
    void handleThreeSixNineToggle(const QJsonObject& data) {
        if (!threeSixNine->canToggleParticipation()) return;
        
        QString bowlerName = data["bowler"].toString();
        bool participating = data["participating"].toBool();
        
        threeSixNine->setBowlerParticipation(bowlerName, participating);
        updateGameDisplay(); // Refresh to show updated status
    }

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
        if (gameStatus) {
            gameStatus->setGameStyleSheet(scheme.background, scheme.foreground);
        }
    }

    void onHoldClicked() {
        if (game) game->holdGame();
    }

    void onSkipClicked() {
        if (game) game->skipPlayer();
    }

    void onResetClicked() {
        if (game) game->resetGame();
    }

    void onCurrentPlayerChanged(const QString& playerName, int index) {
        updateGameDisplay();
        updateGameStatus();
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
    
    void updateGameStatus() {
        if (!gameActive || !game || game->getBowlers().isEmpty() || !gameStatus) {
            if (gameStatus) {
                gameStatus->resetStatus();
            }
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
    GameRecoveryManager* gameRecovery;
    GameStatistics* gameStatistics;
    ThreeSixNineTracker* threeSixNine;
    
    QJsonObject currentGameData;
    ScrollTextWidget* messageScrollArea;
    PinDisplayWidget* pinDisplay;
    
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
