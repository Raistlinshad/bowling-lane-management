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
#include <QMediaPlayer>
#include <QVideoWidget>
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

// Forward declarations
class Bowler;
class Frame;
class Ball;
class QuickGame;
class BowlingMainWindow;

// Ball class representing a single throw
class Ball {
public:
    Ball(const QVector<int>& pins = QVector<int>(5, 0), int value = 0) 
        : pins(pins), value(value) {}
    
    QVector<int> pins;  // [lTwo, lThree, cFive, rThree, rTwo]
    int value;          // Total pin value
};

// Frame class representing a single frame
class Frame {
public:
    Frame() : totalScore(0), isComplete(false), frameScore(0) {}
    
    QVector<Ball> balls;
    int totalScore;     // Running total through this frame
    int frameScore;     // Score for this frame only
    bool isComplete;
    
    bool isStrike() const { return !balls.isEmpty() && balls[0].value == 15; }
    bool isSpare() const { 
        return balls.size() >= 2 && !isStrike() && 
               (balls[0].value + balls[1].value) == 15; 
    }
    
    QString getDisplayText() const {
        if (balls.isEmpty()) return "";
        
        QString result;
        for (int i = 0; i < balls.size(); ++i) {
            if (i > 0) result += " ";
            
            if (balls[i].value == 15) {
                result += "X";  // Strike
            } else if (i > 0 && balls[0].value + balls[i].value == 15) {
                result += "/";  // Spare
            } else {
                result += QString::number(balls[i].value);
            }
        }
        return result;
    }
};

// Bowler class representing a player
class Bowler {
public:
    Bowler(const QString& name) : name(name), currentFrame(0), totalScore(0) {
        frames.resize(10);
    }
    
    QString name;
    QVector<Frame> frames;
    int currentFrame;
    int totalScore;
    
    bool isComplete() const {
        return currentFrame >= 10 || (currentFrame == 9 && frames[9].isComplete);
    }
    
    Frame& getCurrentFrame() { return frames[currentFrame]; }
    const Frame& getCurrentFrame() const { return frames[currentFrame]; }
};

// Pin display widget
class PinDisplayWidget : public QWidget {
    Q_OBJECT
    
public:
    PinDisplayWidget(QWidget* parent = nullptr) : QWidget(parent) {
        setupUI();
        resetPins();
    }
    
    void setPinStates(const QVector<int>& states) {
        pinStates = states;
        updatePinDisplay();
    }
    
    void resetPins() {
        pinStates = QVector<int>(5, 1); // All pins up
        updatePinDisplay();
    }

private slots:
    void updatePinDisplay() {
        // Pin layout: lTwo, lThree, cFive, rThree, rTwo
        const QStringList pinNames = {"L2", "L3", "C5", "R3", "R2"};
        
        for (int i = 0; i < 5; ++i) {
            QLabel* pinLabel = pinLabels[i];
            if (pinStates[i] == 0) {
                // Pin down - black
                pinLabel->setStyleSheet("QLabel { background-color: black; color: white; border: 2px solid white; }");
            } else {
                // Pin up - white
                pinLabel->setStyleSheet("QLabel { background-color: white; color: black; border: 2px solid black; }");
            }
            pinLabel->setText(pinNames[i]);
        }
    }
    
private:
    void setupUI() {
        QGridLayout* layout = new QGridLayout(this);
        layout->setSpacing(5);
        
        // Canadian 5-pin layout
        //     L3
        //  L2    R2
        //     C5
        //     R3
        
        pinLabels.resize(5);
        
        // Create pin labels
        for (int i = 0; i < 5; ++i) {
            pinLabels[i] = new QLabel(this);
            pinLabels[i]->setAlignment(Qt::AlignCenter);
            pinLabels[i]->setMinimumSize(40, 60);
            pinLabels[i]->setMaximumSize(40, 60);
            pinLabels[i]->setFont(QFont("Arial", 10, QFont::Bold));
        }
        
        // Position pins (lTwo=0, lThree=1, cFive=2, rThree=3, rTwo=4)
        layout->addWidget(pinLabels[1], 0, 1);  // L3 top center
        layout->addWidget(pinLabels[0], 1, 0);  // L2 left
        layout->addWidget(pinLabels[4], 1, 2);  // R2 right
        layout->addWidget(pinLabels[2], 2, 1);  // C5 center
        layout->addWidget(pinLabels[3], 3, 1);  // R3 bottom center
    }
    
    QVector<QLabel*> pinLabels;
    QVector<int> pinStates;
};

// Game status widget showing current ball and pins
class GameStatusWidget : public QWidget {
    Q_OBJECT
    
public:
    GameStatusWidget(QWidget* parent = nullptr) : QWidget(parent) {
        setupUI();
    }
    
    void updateStatus(const QString& bowlerName, int frame, int ball, const QVector<int>& pinStates) {
        statusLabel->setText(QString("%1 - Frame %2, Ball %3").arg(bowlerName).arg(frame + 1).arg(ball + 1));
        pinDisplay->setPinStates(pinStates);
    }
    
    void resetStatus() {
        statusLabel->setText("Waiting for game...");
        pinDisplay->resetPins();
    }

private:
    void setupUI() {
        QHBoxLayout* layout = new QHBoxLayout(this);
        
        statusLabel = new QLabel("Waiting for game...", this);
        statusLabel->setFont(QFont("Arial", 16, QFont::Bold));
        statusLabel->setStyleSheet("QLabel { color: white; background-color: blue; padding: 10px; }");
        
        pinDisplay = new PinDisplayWidget(this);
        
        layout->addWidget(statusLabel, 1);
        layout->addWidget(pinDisplay, 0);
    }
    
    QLabel* statusLabel;
    PinDisplayWidget* pinDisplay;
};

// Bowler display widget
class BowlerWidget : public QFrame {
    Q_OBJECT
    
public:
    BowlerWidget(const Bowler& bowler, bool isCurrentPlayer = false, QWidget* parent = nullptr) 
        : QFrame(parent), bowlerData(bowler) {
        setupUI(isCurrentPlayer);
        updateDisplay();
    }
    
    void updateBowler(const Bowler& bowler, bool isCurrentPlayer = false) {
        bowlerData = bowler;
        updateHighlight(isCurrentPlayer);
        updateDisplay();
    }
    
    void updateHighlight(bool isCurrentPlayer) {
        if (isCurrentPlayer) {
            setStyleSheet("QFrame { background-color: yellow; border: 3px solid red; }");
        } else {
            setStyleSheet("QFrame { background-color: lightblue; border: 1px solid black; }");
        }
    }

private:
    void setupUI(bool isCurrentPlayer) {
        setFrameStyle(QFrame::Box);
        updateHighlight(isCurrentPlayer);
        
        QGridLayout* layout = new QGridLayout(this);
        
        // Bowler name
        nameLabel = new QLabel(bowlerData.name, this);
        nameLabel->setFont(QFont("Arial", 20, QFont::Bold));
        nameLabel->setAlignment(Qt::AlignCenter);
        layout->addWidget(nameLabel, 0, 0, 1, 11);
        
        // Frame headers
        QStringList headers = {"F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10", "Total"};
        for (int i = 0; i < headers.size(); ++i) {
            QLabel* header = new QLabel(headers[i], this);
            header->setFont(QFont("Arial", 12, QFont::Bold));
            header->setAlignment(Qt::AlignCenter);
            header->setStyleSheet("QLabel { border: 1px solid black; background-color: lightgray; }");
            layout->addWidget(header, 1, i);
        }
        
        // Frame displays
        frameLabels.resize(10);
        totalLabels.resize(10);
        
        for (int i = 0; i < 10; ++i) {
            // Ball results
            frameLabels[i] = new QLabel(this);
            frameLabels[i]->setAlignment(Qt::AlignCenter);
            frameLabels[i]->setStyleSheet("QLabel { border: 1px solid black; background-color: white; }");
            frameLabels[i]->setMinimumHeight(30);
            layout->addWidget(frameLabels[i], 2, i);
            
            // Frame totals
            totalLabels[i] = new QLabel(this);
            totalLabels[i]->setAlignment(Qt::AlignCenter);
            totalLabels[i]->setStyleSheet("QLabel { border: 1px solid black; background-color: white; }");
            totalLabels[i]->setFont(QFont("Arial", 14, QFont::Bold));
            totalLabels[i]->setMinimumHeight(40);
            layout->addWidget(totalLabels[i], 3, i);
        }
        
        // Total score
        grandTotalLabel = new QLabel(this);
        grandTotalLabel->setAlignment(Qt::AlignCenter);
        grandTotalLabel->setStyleSheet("QLabel { border: 2px solid black; background-color: yellow; }");
        grandTotalLabel->setFont(QFont("Arial", 24, QFont::Bold));
        grandTotalLabel->setMinimumHeight(70);
        layout->addWidget(grandTotalLabel, 2, 10, 2, 1);
    }
    
    void updateDisplay() {
        nameLabel->setText(bowlerData.name);
        
        for (int i = 0; i < 10; ++i) {
            const Frame& frame = bowlerData.frames[i];
            
            // Update ball display
            frameLabels[i]->setText(frame.getDisplayText());
            
            // Update frame total
            if (frame.isComplete) {
                totalLabels[i]->setText(QString::number(frame.totalScore));
            } else if (!frame.balls.isEmpty()) {
                totalLabels[i]->setText("...");
            } else {
                totalLabels[i]->setText("");
            }
        }
        
        // Update grand total
        grandTotalLabel->setText(QString::number(bowlerData.totalScore));
    }
    
    Bowler bowlerData;
    QLabel* nameLabel;
    QVector<QLabel*> frameLabels;
    QVector<QLabel*> totalLabels;
    QLabel* grandTotalLabel;
};

// Media display widget for ads and special effects
class MediaDisplayWidget : public QStackedWidget {
    Q_OBJECT
    
public:
    MediaDisplayWidget(QWidget* parent = nullptr) : QStackedWidget(parent) {
        setupUI();
        setupMediaRotation();
    }
    
    void playSpecialEffect(const QString& effectName, int duration = 3000) {
        qDebug() << "Playing special effect:" << effectName;
        
        if (effectName == "strike") {
            showStrikeEffect();
        } else if (effectName == "spare") {
            showSpareEffect();
        }
        
        // Return to game display after effect
        QTimer::singleShot(duration, this, [this]() {
            setCurrentIndex(0); // Return to game display
        });
    }
    
    void setGameWidget(QWidget* gameWidget) {
        if (indexOf(gameWidget) == -1) {
            insertWidget(0, gameWidget);
        }
        setCurrentIndex(0);
    }

private slots:
    void rotateMedia() {
        if (currentIndex() == 0) return; // Don't rotate if showing game
        
        // Implement media rotation logic here
        // For now, just cycle through available widgets
    }
    
    void showStrikeEffect() {
        QLabel* strikeLabel = new QLabel("STRIKE!", this);
        strikeLabel->setAlignment(Qt::AlignCenter);
        strikeLabel->setStyleSheet("QLabel { background-color: red; color: white; font-size: 72px; font-weight: bold; }");
        
        addWidget(strikeLabel);
        setCurrentWidget(strikeLabel);
        
        // Remove the label after use
        QTimer::singleShot(3000, this, [this, strikeLabel]() {
            removeWidget(strikeLabel);
            strikeLabel->deleteLater();
        });
    }
    
    void showSpareEffect() {
        QLabel* spareLabel = new QLabel("SPARE!", this);
        spareLabel->setAlignment(Qt::AlignCenter);
        spareLabel->setStyleSheet("QLabel { background-color: blue; color: white; font-size: 72px; font-weight: bold; }");
        
        addWidget(spareLabel);
        setCurrentWidget(spareLabel);
        
        // Remove the label after use
        QTimer::singleShot(3000, this, [this, spareLabel]() {
            removeWidget(spareLabel);
            spareLabel->deleteLater();
        });
    }

private:
    void setupUI() {
        // Default placeholder widget
        QLabel* placeholder = new QLabel("Ready for Game", this);
        placeholder->setAlignment(Qt::AlignCenter);
        placeholder->setStyleSheet("QLabel { background-color: blue; color: white; font-size: 48px; }");
        addWidget(placeholder);
    }
    
    void setupMediaRotation() {
        mediaTimer = new QTimer(this);
        connect(mediaTimer, &QTimer::timeout, this, &MediaDisplayWidget::rotateMedia);
        mediaTimer->start(300000); // 5 minutes
    }
    
    QTimer* mediaTimer;
};

// Quick Game implementation
class QuickGame : public QObject {
    Q_OBJECT
    
public:
    QuickGame(QObject* parent = nullptr) : QObject(parent), currentBowlerIndex(0), isHeld(false) {
        machineProcess = new QProcess(this);
        connect(machineProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, &QuickGame::onMachineProcessFinished);
    }
    
    void startGame(const QJsonObject& gameData) {
        qDebug() << "Starting quick game with data:" << gameData;
        
        // Parse bowler names
        QJsonArray playersArray = gameData["players"].toArray();
        bowlers.clear();
        
        for (const QJsonValue& value : playersArray) {
            QString playerName = value.toString();
            bowlers.append(Bowler(playerName));
        }
        
        if (bowlers.isEmpty()) {
            // Default players for testing
            bowlers.append(Bowler("Player 1"));
            bowlers.append(Bowler("Player 2"));
        }
        
        currentBowlerIndex = 0;
        isHeld = false;
        
        // Start machine interface
        startMachineInterface();
        
        emit gameStarted();
        emit currentPlayerChanged(getCurrentBowler().name);
    }
    
    void processBall(const QVector<int>& pins) {
        if (isHeld || bowlers.isEmpty()) return;
        
        qDebug() << "Processing ball with pins:" << pins;
        
        Bowler& currentBowler = bowlers[currentBowlerIndex];
        Frame& currentFrame = currentBowler.getCurrentFrame();
        
        // Calculate pin value (Canadian 5-pin scoring)
        int value = pins[0] * 2 + pins[1] * 3 + pins[2] * 5 + pins[3] * 3 + pins[4] * 2;
        
        Ball newBall(pins, value);
        currentFrame.balls.append(newBall);
        
        // Check for special effects
        if (currentFrame.balls.size() == 1 && value == 15) {
            emit specialEffect("strike");
        } else if (currentFrame.balls.size() == 2 && !currentFrame.isStrike() && currentFrame.isSpare()) {
            emit specialEffect("spare");
        }
        
        // Update game state
        updateScoring();
        checkFrameCompletion();
        
        emit gameUpdated();
    }
    
    void holdGame() {
        isHeld = !isHeld;
        emit gameHeld(isHeld);
        
        if (isHeld) {
            stopMachineInterface();
        } else {
            startMachineInterface();
        }
    }
    
    void skipPlayer() {
        if (bowlers.isEmpty()) return;
        
        // Complete current frame as a miss
        Bowler& currentBowler = bowlers[currentBowlerIndex];
        Frame& currentFrame = currentBowler.getCurrentFrame();
        
        // Fill remaining balls with misses
        while (currentFrame.balls.size() < 3 && !currentFrame.isComplete) {
            currentFrame.balls.append(Ball());
        }
        
        updateScoring();
        checkFrameCompletion();
        nextPlayer();
        
        emit gameUpdated();
    }
    
    void resetGame() {
        for (Bowler& bowler : bowlers) {
            bowler.frames.clear();
            bowler.frames.resize(10);
            bowler.currentFrame = 0;
            bowler.totalScore = 0;
        }
        
        currentBowlerIndex = 0;
        emit gameUpdated();
    }
    
    const QVector<Bowler>& getBowlers() const { return bowlers; }
    int getCurrentBowlerIndex() const { return currentBowlerIndex; }
    const Bowler& getCurrentBowler() const { return bowlers[currentBowlerIndex]; }
    bool isGameHeld() const { return isHeld; }

signals:
    void gameStarted();
    void gameUpdated();
    void gameHeld(bool held);
    void currentPlayerChanged(const QString& playerName);
    void specialEffect(const QString& effect);
    void ballProcessed(const QJsonObject& ballData);

private slots:
    void onMachineProcessFinished(int exitCode, QProcess::ExitStatus exitStatus) {
        Q_UNUSED(exitStatus)
        
        if (exitCode == 0) {
            // Read machine output
            QByteArray output = machineProcess->readAllStandardOutput();
            QJsonDocument doc = QJsonDocument::fromJson(output);
            
            if (doc.isObject()) {
                QJsonObject result = doc.object();
                QJsonArray pinsArray = result["pins"].toArray();
                
                QVector<int> pins;
                for (const QJsonValue& value : pinsArray) {
                    pins.append(value.toInt());
                }
                
                if (pins.size() == 5) {
                    processBall(pins);
                }
            }
        }
    }

private:
    void updateScoring() {
        for (Bowler& bowler : bowlers) {
            int runningTotal = 0;
            
            for (int frameIdx = 0; frameIdx < bowler.frames.size(); ++frameIdx) {
                Frame& frame = bowler.frames[frameIdx];
                
                if (frame.balls.isEmpty()) {
                    frame.frameScore = 0;
                    frame.totalScore = runningTotal;
                    continue;
                }
                
                int frameScore = 0;
                
                if (frameIdx < 9) { // Frames 1-9
                    if (frame.isStrike()) {
                        frameScore = 15;
                        // Add next two balls as bonus
                        if (frameIdx + 1 < bowler.frames.size()) {
                            const Frame& nextFrame = bowler.frames[frameIdx + 1];
                            if (!nextFrame.balls.isEmpty()) {
                                frameScore += nextFrame.balls[0].value;
                                if (nextFrame.balls.size() > 1) {
                                    frameScore += nextFrame.balls[1].value;
                                } else if (nextFrame.balls[0].value == 15 && frameIdx + 2 < bowler.frames.size()) {
                                    // Next frame is also a strike, look ahead
                                    const Frame& nextNextFrame = bowler.frames[frameIdx + 2];
                                    if (!nextNextFrame.balls.isEmpty()) {
                                        frameScore += nextNextFrame.balls[0].value;
                                    }
                                }
                            }
                        }
                    } else if (frame.isSpare()) {
                        frameScore = 15;
                        // Add next ball as bonus
                        if (frameIdx + 1 < bowler.frames.size()) {
                            const Frame& nextFrame = bowler.frames[frameIdx + 1];
                            if (!nextFrame.balls.isEmpty()) {
                                frameScore += nextFrame.balls[0].value;
                            }
                        }
                    } else {
                        // Regular scoring
                        for (const Ball& ball : frame.balls) {
                            frameScore += ball.value;
                        }
                    }
                } else { // Frame 10
                    for (const Ball& ball : frame.balls) {
                        frameScore += ball.value;
                    }
                }
                
                frame.frameScore = frameScore;
                runningTotal += frameScore;
                frame.totalScore = runningTotal;
            }
            
            bowler.totalScore = runningTotal;
        }
    }
    
    void checkFrameCompletion() {
        Bowler& currentBowler = bowlers[currentBowlerIndex];
        Frame& currentFrame = currentBowler.getCurrentFrame();
        
        bool frameComplete = false;
        
        if (currentBowler.currentFrame < 9) { // Frames 1-9
            if (currentFrame.isStrike()) {
                frameComplete = true;
            } else if (currentFrame.balls.size() >= 2) {
                if (currentFrame.isSpare()) {
                    frameComplete = true;
                } else if (currentFrame.balls.size() >= 3) {
                    frameComplete = true;
                }
            }
        } else { // Frame 10
            if (currentFrame.balls.size() >= 3) {
                frameComplete = true;
            } else if (currentFrame.balls.size() == 2) {
                int firstBall = currentFrame.balls[0].value;
                int secondBall = currentFrame.balls[1].value;
                
                if (firstBall < 15 && (firstBall + secondBall) < 15) {
                    frameComplete = true; // Open frame
                }
            }
        }
        
        if (frameComplete) {
            currentFrame.isComplete = true;
            nextPlayer();
        }
    }
    
    void nextPlayer() {
        Bowler& currentBowler = bowlers[currentBowlerIndex];
        
        if (currentBowler.currentFrame < 9) {
            currentBowler.currentFrame++;
        }
        
        // Move to next bowler
        currentBowlerIndex = (currentBowlerIndex + 1) % bowlers.size();
        
        // Check if all bowlers completed current round
        bool allComplete = true;
        int targetFrame = bowlers[0].currentFrame;
        
        for (const Bowler& bowler : bowlers) {
            if (bowler.currentFrame < targetFrame || !bowler.isComplete()) {
                allComplete = false;
                break;
            }
        }
        
        if (allComplete && targetFrame >= 9) {
            emit gameStarted(); // Game complete - could trigger new game
        }
        
        emit currentPlayerChanged(getCurrentBowler().name);
    }
    
    void startMachineInterface() {
        QString pythonScript = "python3 machine_interface.py";
        machineProcess->start(pythonScript);
    }
    
    void stopMachineInterface() {
        if (machineProcess->state() != QProcess::NotRunning) {
            machineProcess->terminate();
            if (!machineProcess->waitForFinished(3000)) {
                machineProcess->kill();
            }
        }
    }
    
    QVector<Bowler> bowlers;
    int currentBowlerIndex;
    bool isHeld;
    QProcess* machineProcess;
};

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
        mediaDisplay->playSpecialEffect(effect);
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

private:
    void setupUI() {
        setWindowTitle("Canadian 5-Pin Bowling");
        setMinimumSize(1200, 800);
        
        QWidget* centralWidget = new QWidget(this);
        setCentralWidget(centralWidget);
        
        QVBoxLayout* mainLayout = new QVBoxLayout(centralWidget);
        
        // Media display area (blue background)
        mediaDisplay = new MediaDisplayWidget(this);
        mediaDisplay->setMinimumHeight(400);
        mediaDisplay->setStyleSheet("MediaDisplayWidget { background-color: blue; }");
        
        // Game display area
        gameDisplayArea = new QScrollArea(this);
        gameDisplayArea->setWidgetResizable(true);
        gameDisplayArea->setMinimumHeight(300);
        
        gameWidget = new QWidget();
        gameLayout = new QVBoxLayout(gameWidget);
        gameDisplayArea->setWidget(gameWidget);
        
        // Set game widget in media display
        mediaDisplay->setGameWidget(gameDisplayArea);
        
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
    
    void onGameCommand(const QString& type, const QJsonObject& data) {
        qDebug() << "Received game command:" << type;
        
        if (type == "quick_game") {
            game->startGame(data);
        } else if (type == "status_update") {
            // Handle status requests
            sendGameStatus();
        } else if (type == "player_update_add") {
            // Add player
            QString playerName = data["player_name"].toString();
            game->addPlayer(playerName);
        } else if (type == "player_update_remove") {
            // Remove player
            QString playerName = data["player_name"].toString();
            game->removePlayer(playerName);
        } else if (type == "score_update") {
            // Update score
            game->updateScore(data);
        } else if (type == "hold_update") {
            // Hold/unhold lane
            bool hold = data["hold"].toBool();
            if (hold != game->isGameHeld()) {
                game->holdGame();
            }
        } else if (type == "move_to") {
            // Move players to another lane
            handleMoveToLane(data);
        } else if (type == "scroll_update") {
            // Update bottom scroll text
            updateScrollText(data["text"].toString());
        }
    }
    
    void sendGameStatus() {
        QJsonObject status;
        status["type"] = "game_status";
        status["lane_id"] = client->getLaneId();
        status["current_player"] = game->getCurrentBowler().name;
        status["game_held"] = game->isGameHeld();
        status["frame"] = game->getCurrentBowler().currentFrame + 1;
        status["ball"] = game->getCurrentBowler().getCurrentFrame().balls.size() + 1;
        
        // Add bowler scores
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
        // Save current game state
        QJsonObject gameState;
        gameState["bowlers"] = serializeBowlers();
        gameState["current_bowler"] = game->getCurrentBowlerIndex();
        gameState["held"] = game->isGameHeld();
        
        // Send to server
        QJsonObject moveMessage;
        moveMessage["type"] = "game_state_transfer";
        moveMessage["target_lane"] = data["target_lane"];
        moveMessage["game_data"] = gameState;
        
        client->sendMessage(moveMessage);
        
        // Reset local game
        game->resetGame();
    }
    
    QJsonArray serializeBowlers() {
        QJsonArray bowlersArray;
        for (const Bowler& bowler : game->getBowlers()) {
            QJsonObject bowlerObj;
            bowlerObj["name"] = bowler.name;
            bowlerObj["total_score"] = bowler.totalScore;
            bowlerObj["current_frame"] = bowler.currentFrame;
            
            QJsonArray framesArray;
            for (const Frame& frame : bowler.frames) {
                QJsonObject frameObj;
                frameObj["total_score"] = frame.totalScore;
                frameObj["frame_score"] = frame.frameScore;
                frameObj["is_complete"] = frame.isComplete;
                
                QJsonArray ballsArray;
                for (const Ball& ball : frame.balls) {
                    QJsonObject ballObj;
                    ballObj["value"] = ball.value;
                    
                    QJsonArray pinsArray;
                    for (int pin : ball.pins) {
                        pinsArray.append(pin);
                    }
                    ballObj["pins"] = pinsArray;
                    ballsArray.append(ballObj);
                }
                frameObj["balls"] = ballsArray;
                framesArray.append(frameObj);
            }
            bowlerObj["frames"] = framesArray;
            bowlersArray.append(bowlerObj);
        }
        return bowlersArray;
    }
    
    void updateScrollText(const QString& text) {
        // Add scroll text display at bottom
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
        
        // Add current player first
        if (!bowlers.isEmpty()) {
            displayOrder.append(currentIdx);
        }
        
        // Add other players
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
        
        // Get current pin states
        QVector<int> pinStates(5, 1); // All pins up by default
        
        if (!currentFrame.balls.isEmpty()) {
            // Show pins knocked down by previous balls in this frame
            for (const Ball& ball : currentFrame.balls) {
                for (int i = 0; i < 5 && i < ball.pins.size(); ++i) {
                    if (ball.pins[i] == 1) {
                        pinStates[i] = 0; // Pin down
                    }
                }
            }
        }
        
        gameStatus->updateStatus(
            currentBowler.name,
            currentBowler.currentFrame,
            currentFrame.balls.size(),
            pinStates
        );
    }

private:
    MediaDisplayWidget* mediaDisplay;
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

// Machine interface helper
class MachineInterface : public QObject {
    Q_OBJECT
    
public:
    MachineInterface(QObject* parent = nullptr) : QObject(parent) {
        pythonProcess = new QProcess(this);
        connect(pythonProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, &MachineInterface::onProcessFinished);
        connect(pythonProcess, &QProcess::readyReadStandardOutput,
                this, &MachineInterface::onDataReady);
    }
    
    void startDetection() {
        if (pythonProcess->state() == QProcess::NotRunning) {
            pythonProcess->start("python3", QStringList() << "machine_interface.py");
        }
    }
    
    void stopDetection() {
        if (pythonProcess->state() != QProcess::NotRunning) {
            pythonProcess->terminate();
            if (!pythonProcess->waitForFinished(3000)) {
                pythonProcess->kill();
            }
        }
    }

signals:
    void ballDetected(const QVector<int>& pins);
    void machineError(const QString& error);

private slots:
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus) {
        Q_UNUSED(exitStatus)
        if (exitCode != 0) {
            emit machineError("Machine process crashed");
        }
    }
    
    void onDataReady() {
        QByteArray data = pythonProcess->readAllStandardOutput();
        QStringList lines = QString::fromUtf8(data).split('\n', Qt::SkipEmptyParts);
        
        for (const QString& line : lines) {
            QJsonDocument doc = QJsonDocument::fromJson(line.toUtf8());
            if (doc.isObject()) {
                QJsonObject obj = doc.object();
                if (obj["type"].toString() == "ball_detected") {
                    QJsonArray pinsArray = obj["pins"].toArray();
                    QVector<int> pins;
                    for (const QJsonValue& value : pinsArray) {
                        pins.append(value.toInt());
                    }
                    if (pins.size() == 5) {
                        emit ballDetected(pins);
                    }
                }
            }
        }
    }

private:
    QProcess* pythonProcess;
};

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    
    // Set application properties
    app.setApplicationName("Canadian5PinBowling");
    app.setApplicationVersion("1.0");
    app.setOrganizationName("BowlingCenter");
    
    BowlingMainWindow window;
    window.show();
    
    return app.exec();
}

#include "main.moc"