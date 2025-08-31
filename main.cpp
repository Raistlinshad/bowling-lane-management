#include <QApplication>
#include <QMainWindow>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QFrame>
#include <QScrollArea>
#include <QThreadPool>
#include <QPixmapCache>
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


// Main bowling window class
class BowlingMainWindow : public QMainWindow {
    Q_OBJECT
    
public:
    BowlingMainWindow(QWidget* parent = nullptr) : QMainWindow(parent), 
        gameActive(false), currentGameNumber(1), gameOver(false), isCallMode(false),
        framesSinceFirstBall(0), flashing(false) {
        
        // Initialize systems
        gameRecovery = new GameRecoveryManager(this);
        gameStatistics = new GameStatistics(this);
        gameStatus = new GameStatusWidget(this);
        threeSixNine = new ThreeSixNineTracker(this);
        
        // Initialize call timer
        callTimer = new QTimer(this);
        callTimer->setSingleShot(false);
        callTimer->setInterval(500); // Flash every 500ms
        connect(callTimer, &QTimer::timeout, this, &BowlingMainWindow::onCallFlash);
        
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

    void onCurrentPlayerChanged(const QString& playerName, int index) {
        updateGameDisplay();
        updateGameStatus();
    }

private:
    void setupGameInterface() {
        QVBoxLayout* gameLayout = new QVBoxLayout(gameInterfaceWidget);
        gameLayout->setContentsMargins(0, 0, 0, 0); // Remove all margins
        gameLayout->setSpacing(0); // Remove spacing
    
        // MAIN GAME AREA - Set to fill entire available space
        gameDisplayArea = new QScrollArea(this);
        gameDisplayArea->setWidgetResizable(true);
        gameDisplayArea->setStyleSheet("QScrollArea { border: none; background-color: #2b2b2b; }");
    
        gameWidget = new QWidget();
        gameWidgetLayout = new QVBoxLayout(gameWidget);
        gameWidgetLayout->setContentsMargins(10, 0, 10, 50); // Bottom margin for button space
        gameDisplayArea->setWidget(gameWidget);
    
        // BOTTOM CONTROL BAR - Add directly to gameWidget layout, not separate widget
        QHBoxLayout* bottomBarLayout = new QHBoxLayout();
        bottomBarLayout->setSpacing(10);
        bottomBarLayout->setContentsMargins(10, 5, 10, 5);
    
        // Left: Control Buttons
        holdButton = new QPushButton("HOLD", this);
        skipButton = new QPushButton("SKIP", this);
        resetButton = new QPushButton("RESET", this);
    
        holdButton->setFixedSize(100, 40);
        skipButton->setFixedSize(100, 40);
        resetButton->setFixedSize(100, 40);
    
        connect(holdButton, &QPushButton::clicked, this, &BowlingMainWindow::onHoldClicked);
        connect(skipButton, &QPushButton::clicked, this, &BowlingMainWindow::onSkipClicked);
        connect(resetButton, &QPushButton::clicked, this, &BowlingMainWindow::onResetClicked);
    
        bottomBarLayout->addWidget(holdButton);
        bottomBarLayout->addWidget(skipButton);
        bottomBarLayout->addWidget(resetButton);
        bottomBarLayout->addSpacing(20); // Space between buttons and message area
    
        // Center: Scrolling Message Area
        messageScrollArea = new ScrollTextWidget(this);
        messageScrollArea->setText("Welcome to Canadian 5-Pin Bowling");
        messageScrollArea->setFixedHeight(40);
        messageScrollArea->setStyleSheet("QLabel { background-color: black; color: yellow; font-size: 14px; border: 1px solid #555555; }");
    
        // Right: Pin Display
        pinDisplay = new PinDisplayWidget(this);
        pinDisplay->setDisplayMode("small");
        pinDisplay->setFixedSize(120, 40);
        
        // Far Right: Lane Status for call mode
        laneStatusLabel = new QLabel(QString("Lane %1").arg(1), this);
        laneStatusLabel->setFixedSize(80, 40);
        laneStatusLabel->setAlignment(Qt::AlignCenter);
        laneStatusLabel->setStyleSheet("QLabel { color: white; font-size: 18px; font-weight: bold; background-color: black; }");
    
        bottomBarLayout->addWidget(messageScrollArea, 1);     // Expandable message area
        bottomBarLayout->addSpacing(10);
        // bottomBarLayout->addWidget(pinDisplay);
        bottomBarLayout->addWidget(laneStatusLabel);
    
        // Create a bottom bar widget that sits at the bottom of the game widget
        QWidget* bottomBarContainer = new QWidget();
        bottomBarContainer->setFixedHeight(50);
        bottomBarContainer->setStyleSheet("QWidget { background-color: black; }");
        bottomBarContainer->setLayout(bottomBarLayout);
        
        // Add bottom bar to the game widget layout (so it appears inside the scroll area at the bottom)
        gameWidgetLayout->addStretch(); // Push everything up
        gameWidgetLayout->addWidget(bottomBarContainer); // Add bottom bar at the end
    
        // Assemble main layout - only the scroll area, no separate bottom widget
        gameLayout->addWidget(gameDisplayArea, 1);    // Takes entire space
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
        if (laneStatusLabel) {
            laneStatusLabel->setStyleSheet("QLabel { color: white; font-size: 18px; font-weight: bold; background-color: black; }");
        }
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
    if (!gameActive || !game) {
        qDebug() << "Game not active or null, skipping display update";
        return;
    }
    
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
    
    // FIXED: Safe pin display update with validation and error handling
    if (pinDisplay) {
        try {
            QVector<int> pinStates;
            
            // Get pin states with safety checks
            if (game) {
                pinStates = game->getCurrentPinStates();
                
                // Validate the pin states before setting
                if (pinStates.size() == 5) {
                    // Validate each pin state value
                    bool validStates = true;
                    for (int i = 0; i < pinStates.size(); ++i) {
                        if (pinStates[i] < 0 || pinStates[i] > 1) {
                            qWarning() << "Invalid pin state at index" << i << "value:" << pinStates[i];
                            validStates = false;
                            break;
                        }
                    }
                    
                    if (validStates) {
                        // Safe to set pin states
                        pinDisplay->setPinStates(pinStates);
                    } else {
                        // Use default safe pin states
                        qWarning() << "Using default pin states due to invalid values";
                        QVector<int> defaultStates = {1, 1, 1, 1, 1}; // All pins up
                        pinDisplay->setPinStates(defaultStates);
                    }
                } else {
                    qWarning() << "Invalid pin states size:" << pinStates.size() << "expected 5";
                    // Use default safe pin states
                    QVector<int> defaultStates = {1, 1, 1, 1, 1}; // All pins up
                    pinDisplay->setPinStates(defaultStates);
                }
            } else {
                qWarning() << "Game object is null, using default pin states";
                QVector<int> defaultStates = {1, 1, 1, 1, 1}; // All pins up
                pinDisplay->setPinStates(defaultStates);
            }
        } catch (const std::exception& e) {
            qWarning() << "Exception in pin display update:" << e.what();
            // Emergency fallback - reset pins to default state
            QVector<int> safeStates = {1, 1, 1, 1, 1}; // All pins up
            try {
                pinDisplay->setPinStates(safeStates);
            } catch (...) {
                qWarning() << "Critical: Cannot update pin display, disabling for this session";
                // Disable pin display updates for remainder of session
                pinDisplay->setVisible(false);
            }
        } catch (...) {
            qWarning() << "Unknown exception in pin display update";
            // Emergency fallback
            QVector<int> safeStates = {1, 1, 1, 1, 1}; // All pins up
            try {
                pinDisplay->setPinStates(safeStates);
            } catch (...) {
                qWarning() << "Critical: Cannot update pin display, disabling for this session";
                pinDisplay->setVisible(false);
            }
        }
    } else {
        qWarning() << "pinDisplay is null";
    }
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
        
        // Create main layout with no margins or spacing
        QVBoxLayout* mainLayout = new QVBoxLayout(centralWidget);
        mainLayout->setContentsMargins(0, 0, 0, 0);
        mainLayout->setSpacing(0);
        
        // Media display area (full screen when no game)
        mediaDisplay = new MediaManager(this);
        
        // Game interface container (hidden by default)
        gameInterfaceWidget = new QWidget(this);
        gameInterfaceWidget->hide();
        setupGameInterface();
        
        // Layout assembly - both widgets take full space when visible
        mainLayout->addWidget(mediaDisplay, 1);
        mainLayout->addWidget(gameInterfaceWidget, 1);
        
        // Start with media display only
        mediaDisplay->showMediaRotation();
    }
    
    void showGameInterface() {
        qDebug() << "=== SHOWING GAME INTERFACE ===";
        
        // Hide the media display completely during game
        mediaDisplay->hide();
        
        // Show game interface and make it fill entire window
        gameInterfaceWidget->show();
        gameInterfaceWidget->setParent(centralWidget());
        
        // Update layout to give game interface full space
        QVBoxLayout* mainLayout = static_cast<QVBoxLayout*>(centralWidget()->layout());
        mainLayout->setContentsMargins(0, 0, 0, 0);
        mainLayout->setSpacing(0);
        
        qDebug() << "=== GAME INTERFACE DISPLAY COMPLETE ===";
    }
    
    void hideGameInterface() {
        // Show media display and hide game interface
        mediaDisplay->show();
        gameInterfaceWidget->hide();
        
        // Restore normal layout spacing
        QVBoxLayout* mainLayout = static_cast<QVBoxLayout*>(centralWidget()->layout());
        mainLayout->setContentsMargins(5, 5, 5, 5);
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
        
        // Update lane status label with actual lane ID
        if (laneStatusLabel) {
            laneStatusLabel->setText(QString("Lane %1").arg(laneId));
        }
    }
    
    void setupGame() {
        qDebug() << "=== SETTING UP GAME ===";
    
        game = new QuickGame(this);
        qDebug() << "Created QuickGame instance";
    
        // Connect signals
        connect(game, &QuickGame::gameUpdated, this, &BowlingMainWindow::onGameUpdated);
        connect(game, &QuickGame::specialEffect, this, &BowlingMainWindow::onSpecialEffect);
        connect(game, &QuickGame::currentPlayerChanged, this, &BowlingMainWindow::onCurrentPlayerChanged);
        connect(game, &QuickGame::gameStarted, this, &BowlingMainWindow::onGameStarted);
        connect(game, &QuickGame::gameEnded, this, &BowlingMainWindow::onGameEnded);
        connect(game, &QuickGame::ballProcessed, this, &BowlingMainWindow::onBallProcessed);
        connect(game, &QuickGame::gameHeld, this, [this](bool held) {
            qDebug() << "Game hold state changed to:" << held;
            updateButtonStates();
        });
    
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
    QLabel* laneStatusLabel; // For call mode flashing
    
    QPushButton* holdButton;
    QPushButton* skipButton;
    QPushButton* resetButton;
    
    LaneClient* client;
    QuickGame* game;
    
    // Game state
    bool gameActive;
    bool gameOver;
    bool isCallMode;
    bool flashing;
    QString currentGameType;
    int currentGameNumber;
    int framesSinceFirstBall;
    QVector<ColorScheme> gameColors;
    
    // Timers
    QTimer* callTimer;
};

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    
    app.setApplicationName("Canadian5PinBowling");
    app.setApplicationVersion("1.0");
    app.setOrganizationName("BowlingCenter");
    
    // Raspberry Pi 3 optimizations
    QThreadPool::globalInstance()->setMaxThreadCount(2); // Limit to 2 threads
    app.setAttribute(Qt::AA_DisableWindowContextHelpButton);
    app.setQuitOnLastWindowClosed(true);
    QPixmapCache::setCacheLimit(1024); // Reduce memory usage
    
    BowlingMainWindow window;
    window.show();
    
    return app.exec();
}

#include "main.moc"


