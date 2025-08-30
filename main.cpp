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
#include "MediaManager.h"
#include "BowlingWidgets.h"
#include "ThreeSixNineTracker.h"
#include "GameStatistics.h"
#include "GameRecoveryManager.h"


// Main bowling window
class BowlingMainWindow : public QMainWindow {
    Q_OBJECT
    
public:
    BowlingMainWindow(QWidget* parent = nullptr) : QMainWindow(parent), 
        gameActive(false), currentGameNumber(1), gameOver(false), isCallMode(false),
        framesSinceFirstBall(0), callTimer(nullptr), flashing(false) {
        
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
        
        // Initialize call mode timer
        callTimer = new QTimer(this);
        callTimer->setSingleShot(false);
        callTimer->setInterval(500); // Flash every 500ms
        connect(callTimer, &QTimer::timeout, this, &BowlingMainWindow::onCallFlash);
        
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
        updateButtonStates();
        
        // Save game state for recovery
        if (gameActive && game && !gameOver) {
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
        gameOver = false;
        isCallMode = false;
        framesSinceFirstBall = 0;
        
        showGameInterface();
        applyGameColors();
        updateButtonStates();
        
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
        gameOver = true;
        isCallMode = false;
        callTimer->stop();
        currentGameNumber++;
        
        updateButtonStates();
        
        // Show completion message
        QString completionMsg = QString("Game %1 Complete! Thank you for playing.").arg(currentGameNumber - 1);
        messageScrollArea->setText(completionMsg);
        messageScrollArea->startScrolling();
        
        // Wait before returning to media rotation
        QTimer::singleShot(10000, this, [this]() {
            if (gameOver) { // Still in game over state
                hideGameInterface();
                mediaDisplay->showMediaRotation();
                gameOver = false;
            }
        });
    }
    
    void onBallProcessed(const QJsonObject& ballData) {
        QString bowlerName = ballData["bowler"].toString();
        int frame = ballData["frame"].toInt();
        int ballValue = ballData["value"].toInt();
        bool isStrike = (ballValue == 15);
        bool isSpare = ballData.contains("is_spare") ? ballData["is_spare"].toBool() : false;
        
        // Count frames since first ball for button state management
        framesSinceFirstBall++;
        
        // Record for statistics
        if (game) {
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
        updateButtonStates();
    }
    
    void onCallFlash() {
        if (isCallMode) {
            flashing = !flashing;
            QString laneText = QString("Lane %1").arg(client->getLaneId());
            if (flashing) {
                laneStatusLabel->setText(laneText);
                laneStatusLabel->setStyleSheet("QLabel { color: red; font-size: 18px; font-weight: bold; background-color: yellow; }");
            } else {
                laneStatusLabel->setText(laneText);
                laneStatusLabel->setStyleSheet("QLabel { color: white; font-size: 18px; font-weight: bold; background-color: black; }");
            }
        }
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
            
        } else if (type == "close_game") {
            handleCloseGame();
            
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
    
        // BOTTOM CONTROL BAR (horizontal layout with reduced height and black background)
        QHBoxLayout* bottomBarLayout = new QHBoxLayout();
        bottomBarLayout->setSpacing(10);
        bottomBarLayout->setContentsMargins(5, 2, 5, 2); // Reduced vertical margins
    
        // Left: Control Buttons (reduced size)
        QHBoxLayout* buttonLayout = new QHBoxLayout();
        holdButton = new QPushButton("HOLD", this);
        skipButton = new QPushButton("SKIP", this);
        resetButton = new QPushButton("RESET", this);
    
        holdButton->setMinimumSize(100, 40); // Reduced from 120x60
        skipButton->setMinimumSize(100, 40);
        resetButton->setMinimumSize(100, 40);
    
        connect(holdButton, &QPushButton::clicked, this, &BowlingMainWindow::onHoldClicked);
        connect(skipButton, &QPushButton::clicked, this, &BowlingMainWindow::onSkipClicked);
        connect(resetButton, &QPushButton::clicked, this, &BowlingMainWindow::onResetClicked);
    
        buttonLayout->addWidget(holdButton);
        buttonLayout->addWidget(skipButton);
        buttonLayout->addWidget(resetButton);
    
        // Center: Scrolling Message Area (reduced height)
        messageScrollArea = new ScrollTextWidget(this);
        messageScrollArea->setText("Welcome to Canadian 5-Pin Bowling");
        messageScrollArea->setMinimumHeight(40); // Reduced from 60
        messageScrollArea->setStyleSheet("QLabel { background-color: black; color: yellow; font-size: 14px; border: 1px solid #555555; }");
    
        // Right: Pin Display (reduced size)
        pinDisplay = new PinDisplayWidget(this);
        pinDisplay->setDisplayMode("small");
        pinDisplay->setMinimumSize(120, 40); // Reduced from 140x60
        pinDisplay->setMaximumSize(140, 40);
        
        // Far Right: Lane Status for call mode
        laneStatusLabel = new QLabel(QString("Lane %1").arg(client ? client->getLaneId() : 1), this);
        laneStatusLabel->setMinimumSize(80, 40);
        laneStatusLabel->setAlignment(Qt::AlignCenter);
        laneStatusLabel->setStyleSheet("QLabel { color: white; font-size: 18px; font-weight: bold; background-color: black; }");
    
        // Assemble bottom bar with black background
        QWidget* bottomBarWidget = new QWidget(this);
        bottomBarWidget->setStyleSheet("QWidget { background-color: black; }");
        bottomBarWidget->setMaximumHeight(50); // Limit height
        bottomBarWidget->setLayout(bottomBarLayout);
        
        bottomBarLayout->addLayout(buttonLayout, 0);          // Fixed width buttons
        bottomBarLayout->addWidget(messageScrollArea, 1);     // Expandable message area
        bottomBarLayout->addWidget(pinDisplay, 0);            // Fixed width pin display
        bottomBarLayout->addWidget(laneStatusLabel, 0);       // Fixed width lane status
    
        // Assemble main layout
        gameLayout->addWidget(gameDisplayArea, 1);    // Takes most space
        gameLayout->addWidget(bottomBarWidget, 0);    // Fixed height bottom bar
    }
    
    void updateButtonStates() {
        if (!gameActive || gameOver) {
            // Game over state - only CALL button active
            holdButton->setText("CALL");
            holdButton->setEnabled(true);
            holdButton->setStyleSheet("QPushButton { background-color: orange; color: black; font-size: 14px; font-weight: bold; }");
            
            skipButton->setEnabled(false);
            skipButton->setStyleSheet("QPushButton { background-color: #666666; color: #999999; font-size: 14px; }");
            
            resetButton->setEnabled(false);
            resetButton->setStyleSheet("QPushButton { background-color: #666666; color: #999999; font-size: 14px; }");
            
            return;
        }
        
        // Normal game state
        if (isCallMode) {
            holdButton->setText("CALL");
            holdButton->setStyleSheet("QPushButton { background-color: red; color: white; font-size: 14px; font-weight: bold; }");
        } else if (game && game->isGameHeld()) {
            holdButton->setText("RESUME");
            holdButton->setStyleSheet("QPushButton { background-color: green; color: white; font-size: 14px; font-weight: bold; }");
        } else {
            holdButton->setText("HOLD");
            holdButton->setStyleSheet("QPushButton { background-color: blue; color: white; font-size: 14px; font-weight: bold; }");
        }
        
        // Reset button logic: RESET for first ball, SET after first ball
        if (framesSinceFirstBall == 0) {
            resetButton->setText("RESET");
        } else {
            resetButton->setText("SET");
        }
        resetButton->setEnabled(true);
        resetButton->setStyleSheet("QPushButton { background-color: darkred; color: white; font-size: 14px; font-weight: bold; }");
        
        // Skip button
        skipButton->setEnabled(true);
        skipButton->setStyleSheet("QPushButton { background-color: orange; color: black; font-size: 14px; font-weight: bold; }");
    }
    
    void handleCloseGame() {
        qDebug() << "Received close game command";
        
        if (gameActive) {
            game->endGame();
        }
        
        gameOver = false;
        isCallMode = false;
        callTimer->stop();
        hideGameInterface();
        mediaDisplay->showMediaRotation();
        
        // Reset lane status
        laneStatusLabel->setStyleSheet("QLabel { color: white; font-size: 18px; font-weight: bold; background-color: black; }");
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
        
        // Create enhanced bowler widgets - CURRENT PLAYER FIRST
        QVector<EnhancedBowlerWidget*> widgetOrder;
        
        // Add current player first
        if (currentIdx >= 0 && currentIdx < bowlers.size()) {
            QJsonObject displayOptions;
            if (currentGameData.contains("display_options")) {
                displayOptions = currentGameData["display_options"].toObject();
            }
            
            // Add 3-6-9 status if active
            if (threeSixNine->isActive()) {
                displayOptions["three_six_nine_status"] = threeSixNine->getStatusText(bowlers[currentIdx].name);
                displayOptions["three_six_nine_dots"] = threeSixNine->getDotsCount(bowlers[currentIdx].name);
            }
            
            EnhancedBowlerWidget* currentWidget = new EnhancedBowlerWidget(bowlers[currentIdx], true, displayOptions);
            currentWidget->setStyleSheet("QFrame { border: 3px solid red; background-color: black; color: red; }");
            widgetOrder.append(currentWidget);
        }
        
        // Add other players
        for (int i = 0; i < bowlers.size(); ++i) {
            if (i != currentIdx) { // Skip current player as already added
                QJsonObject displayOptions;
                if (currentGameData.contains("display_options")) {
                    displayOptions = currentGameData["display_options"].toObject();
                }
                
                // Add 3-6-9 status if active
                if (threeSixNine->isActive()) {
                    displayOptions["three_six_nine_status"] = threeSixNine->getStatusText(bowlers[i].name);
                    displayOptions["three_six_nine_dots"] = threeSixNine->getDotsCount(bowlers[i].name);
                }
                
                EnhancedBowlerWidget* otherWidget = new EnhancedBowlerWidget(bowlers[i], false, displayOptions);
                otherWidget->setStyleSheet("QFrame { border: 1px solid lightblue; background-color: black; color: lightblue; }");
                widgetOrder.append(otherWidget);
            }
        }
        
        // Add widgets to layout in order
        for (EnhancedBowlerWidget* widget : widgetOrder) {
            gameWidgetLayout->addWidget(widget);
        }
        
        gameWidgetLayout->addStretch();
        
        // Update pin display
        QVector<int> pinStates = game->getCurrentPinStates();
        pinDisplay->setPinStates(pinStates);
    }

    void onHoldClicked() {
        if (gameOver || !gameActive) {
            // Game over - CALL mode
            isCallMode = !isCallMode;
            if (isCallMode) {
                callTimer->start();
                // Send hold command to server (same as regular hold)
                if (game) game->holdGame();
            } else {
                callTimer->stop();
                laneStatusLabel->setStyleSheet("QLabel { color: white; font-size: 18px; font-weight: bold; background-color: black; }");
            }
        } else {
            // Normal game - hold/resume
            if (game) game->holdGame();
        }
        updateButtonStates();
    }

    void onSkipClicked() {
        if (game && gameActive && !gameOver) game->skipPlayer();
    }

    void onResetClicked() {
        if (!game || !gameActive || gameOver) return;
        
        // Send machine reset/set command instead of resetting game
        QJsonObject resetData;
        resetData["immediate"] = true;
        
        if (framesSinceFirstBall == 0) {
            // First ball - full reset
            resetData["reset_type"] = "FULL_RESET";
            game->sendMachineCommand("machine_reset", resetData);
        } else {
            // After first ball - set pins to current state
            resetData["reset_type"] = "SET_PINS";
            QVector<int> currentPins = game->getCurrentPinStates();
            QJsonArray pinsArray;
            for (int pin : currentPins) {
                pinsArray.append(pin);
            }
            resetData["pin_states"] = pinsArray;
            game->sendMachineCommand("machine_set_pins", resetData);
        }
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
        mediaDisplay->setMaximumHeight(400);
        gameInterfaceWidget->show();
        mediaDisplay->showGameDisplay(gameDisplayArea);
        qDebug() << "=== GAME INTERFACE DISPLAY COMPLETE ===";
    }
    
    void hideGameInterface() {
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
            updateButtonStates();
        });
        qDebug() << "Connected gameHeld signal:" << connected;
    
        qDebug() << "=== GAME SETUP COMPLETE ===";
    }
    
    void applyDarkTheme() {
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
            QPushButton:disabled {
                background-color: #666666;
                color: #999999;
                border-color: #555555;
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
        settings.endGroup();
    }
    
    void applyGameColors() {
        if (gameColors.isEmpty()) return;
    
        int colorIndex = (currentGameNumber - 1) % gameColors.size();
        const ColorScheme& scheme = gameColors[colorIndex];
    
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
    
        if (gameStatus) {
            gameStatus->setGameStyleSheet(scheme.background, scheme.foreground);
        }

    }

#include "main.moc"
