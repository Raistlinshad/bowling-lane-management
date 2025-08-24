#include "QuickGame.h"
#include <QDebug>
#include <QJsonDocument>
#include <QJsonArray>
#include <QDateTime>

// Static constants
const QVector<int> QuickGame::PIN_VALUES = {2, 3, 5, 3, 2}; // lTwo, lThree, cFive, rThree, rTwo

// Ball class implementation
Ball::Ball(const QVector<int>& pins, int value) : pins(pins), value(value) {
    if (value == 0 && !pins.isEmpty()) {
        this->value = calculateValue(pins);
    }
}

int Ball::calculateValue(const QVector<int>& pins) {
    if (pins.size() != 5) return 0;
    
    int total = 0;
    for (int i = 0; i < 5; ++i) {
        if (pins[i] == 1) { // Pin down
            total += QuickGame::PIN_VALUES[i];
        }
    }
    return total;
}

// Frame class implementation
Frame::Frame() : totalScore(0), isComplete(false), frameScore(0) {}

bool Frame::isStrike() const {
    return !balls.isEmpty() && balls[0].value == 15;
}

bool Frame::isSpare() const {
    if (balls.size() < 2 || isStrike()) return false;
    
    int total = 0;
    for (const Ball& ball : balls) {
        total += ball.value;
    }
    return total == 15;
}

bool Frame::isOpen() const {
    if (balls.isEmpty()) return true;
    
    int total = 0;
    for (const Ball& ball : balls) {
        total += ball.value;
    }
    return total < 15;
}

QString Frame::getDisplayText() const {
    if (balls.isEmpty()) return "";
    
    QString result;
    for (int i = 0; i < balls.size(); ++i) {
        if (i > 0) result += " ";
        
        if (balls[i].value == 15) {
            result += "X"; // Strike
        } else if (i > 0 && !isStrike()) {
            // Check if this ball makes a spare
            int runningTotal = 0;
            for (int j = 0; j <= i; ++j) {
                runningTotal += balls[j].value;
            }
            if (runningTotal == 15) {
                result += "/"; // Spare
            } else {
                result += QString::number(balls[i].value);
            }
        } else {
            result += QString::number(balls[i].value);
        }
    }
    return result;
}

bool Frame::shouldComplete(int frameNumber) const {
    if (frameNumber < 9) { // Frames 1-9
        if (isStrike()) return true;
        if (balls.size() >= 2) {
            if (isSpare()) return true;
            if (balls.size() >= 3) return true;
        }
    } else { // Frame 10
        if (balls.size() >= 3) return true;
        if (balls.size() == 2) {
            int total = balls[0].value + balls[1].value;
            if (balls[0].value < 15 && total < 15) {
                return true; // Open frame
            }
        }
    }
    return false;
}

int Frame::getFrameTotal() const {
    int total = 0;
    for (const Ball& ball : balls) {
        total += ball.value;
    }
    return total;
}

bool Frame::needsBonus() const {
    return isStrike() || isSpare();
}

int Frame::getBonusBalls() const {
    if (isStrike()) return 2;
    if (isSpare()) return 1;
    return 0;
}

// Bowler class implementation
Bowler::Bowler(const QString& name) : name(name), currentFrame(0), totalScore(0) {
    frames.resize(10);
}

bool Bowler::isComplete() const {
    return currentFrame >= 10 || (currentFrame == 9 && frames[9].isComplete);
}

Frame& Bowler::getCurrentFrame() {
    if (currentFrame >= frames.size()) {
        currentFrame = frames.size() - 1;
    }
    return frames[currentFrame];
}

const Frame& Bowler::getCurrentFrame() const {
    if (currentFrame >= frames.size()) {
        return frames.last();
    }
    return frames[currentFrame];
}

void Bowler::nextFrame() {
    if (currentFrame < 9) {
        currentFrame++;
    }
}

void Bowler::reset() {
    frames.clear();
    frames.resize(10);
    currentFrame = 0;
    totalScore = 0;
}

QJsonObject Bowler::toJson() const {
    QJsonObject obj;
    obj["name"] = name;
    obj["current_frame"] = currentFrame;
    obj["total_score"] = totalScore;
    
    QJsonArray framesArray;
    for (const Frame& frame : frames) {
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
    obj["frames"] = framesArray;
    
    return obj;
}

void Bowler::fromJson(const QJsonObject& json) {
    name = json["name"].toString();
    currentFrame = json["current_frame"].toInt();
    totalScore = json["total_score"].toInt();
    
    frames.clear();
    frames.resize(10);
    
    QJsonArray framesArray = json["frames"].toArray();
    for (int i = 0; i < framesArray.size() && i < 10; ++i) {
        QJsonObject frameObj = framesArray[i].toObject();
        Frame& frame = frames[i];
        
        frame.totalScore = frameObj["total_score"].toInt();
        frame.frameScore = frameObj["frame_score"].toInt();
        frame.isComplete = frameObj["is_complete"].toBool();
        
        QJsonArray ballsArray = frameObj["balls"].toArray();
        for (const QJsonValue& ballValue : ballsArray) {
            QJsonObject ballObj = ballValue.toObject();
            
            QVector<int> pins;
            QJsonArray pinsArray = ballObj["pins"].toArray();
            for (const QJsonValue& pinValue : pinsArray) {
                pins.append(pinValue.toInt());
            }
            
            Ball ball(pins, ballObj["value"].toInt());
            frame.balls.append(ball);
        }
    }
}

// MachineInterface class implementation
MachineInterface::MachineInterface(QObject* parent) 
    : QObject(parent), pythonProcess(nullptr), heartbeatTimer(nullptr), machineIsReady(false) {
    setupProcess();
}

MachineInterface::~MachineInterface() {
    stopDetection();
}

void MachineInterface::setupProcess() {
    pythonProcess = new QProcess(this);
    
    connect(pythonProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &MachineInterface::onProcessFinished);
    connect(pythonProcess, &QProcess::readyReadStandardOutput,
            this, &MachineInterface::onDataReady);
    connect(pythonProcess, &QProcess::errorOccurred,
            this, &MachineInterface::onErrorOccurred);
    
    heartbeatTimer = new QTimer(this);
    heartbeatTimer->setInterval(30000); // 30 seconds
    connect(heartbeatTimer, &QTimer::timeout, this, [this]() {
        sendCommand("ping");
    });
}

void MachineInterface::startDetection() {
    if (pythonProcess->state() == QProcess::NotRunning) {
        qDebug() << "Starting machine interface process";
        pythonProcess->start("python3", QStringList() << "machine_interface.py");
        
        if (pythonProcess->waitForStarted(5000)) {
            heartbeatTimer->start();
            sendCommand("start_detection");
            emit machineStatusChanged("starting");
        } else {
            emit machineError("Failed to start machine interface process");
        }
    }
}

void MachineInterface::stopDetection() {
    if (pythonProcess && pythonProcess->state() != QProcess::NotRunning) {
        sendCommand("stop_detection");
        
        pythonProcess->terminate();
        if (!pythonProcess->waitForFinished(3000)) {
            pythonProcess->kill();
            pythonProcess->waitForFinished(1000);
        }
    }
    
    if (heartbeatTimer) {
        heartbeatTimer->stop();
    }
    
    machineIsReady = false;
    emit machineStatusChanged("stopped");
}

bool MachineInterface::isRunning() const {
    return pythonProcess && pythonProcess->state() == QProcess::Running && machineIsReady;
}

void MachineInterface::sendCommand(const QString& command, const QJsonObject& data) {
    if (!pythonProcess || pythonProcess->state() != QProcess::Running) {
        qWarning() << "Cannot send command - machine interface not running";
        return;
    }
    
    QJsonObject cmd;
    cmd["type"] = command;
    if (!data.isEmpty()) {
        cmd["data"] = data;
    }
    cmd["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    
    QJsonDocument doc(cmd);
    QByteArray cmdData = doc.toJson(QJsonDocument::Compact) + "\n";
    
    pythonProcess->write(cmdData);
    pythonProcess->waitForBytesWritten(1000);
}

void MachineInterface::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus) {
    Q_UNUSED(exitStatus)
    
    machineIsReady = false;
    heartbeatTimer->stop();
    
    if (exitCode != 0) {
        emit machineError(QString("Machine process exited with code %1").arg(exitCode));
    }
    
    emit machineStatusChanged("stopped");
}

void MachineInterface::onDataReady() {
    QByteArray data = pythonProcess->readAllStandardOutput();
    QStringList lines = QString::fromUtf8(data).split('\n', Qt::SkipEmptyParts);
    
    for (const QString& line : lines) {
        processMachineOutput(line.trimmed());
    }
}

void MachineInterface::onErrorOccurred(QProcess::ProcessError error) {
    QString errorString;
    switch (error) {
        case QProcess::FailedToStart:
            errorString = "Failed to start machine interface";
            break;
        case QProcess::Crashed:
            errorString = "Machine interface crashed";
            break;
        case QProcess::Timedout:
            errorString = "Machine interface timed out";
            break;
        default:
            errorString = "Unknown machine interface error";
            break;
    }
    
    machineIsReady = false;
    emit machineError(errorString);
    emit machineStatusChanged("error");
}

void MachineInterface::processMachineOutput(const QString& line) {
    if (line.isEmpty()) return;
    
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(line.toUtf8(), &error);
    
    if (error.error != QJsonParseError::NoError) {
        qWarning() << "Invalid JSON from machine interface:" << error.errorString();
        return;
    }
    
    QJsonObject obj = doc.object();
    QString type = obj["type"].toString();
    
    if (type == "ball_detected") {
        QJsonArray pinsArray = obj["pins"].toArray();
        QVector<int> pins;
        for (const QJsonValue& value : pinsArray) {
            pins.append(value.toInt());
        }
        if (pins.size() == 5) {
            emit ballDetected(pins);
        }
    } else if (type == "machine_ready") {
        machineIsReady = true;
        emit machineReady();
        emit machineStatusChanged("ready");
    } else if (type == "error") {
        lastError = obj["message"].toString();
        emit machineError(lastError);
    } else if (type == "pong") {
        // Heartbeat response - connection is alive
    } else if (type == "status") {
        machineIsReady = obj["machine_initialized"].toBool() && obj["detection_active"].toBool();
        emit machineStatusChanged(machineIsReady ? "ready" : "not_ready");
    }
}

// QuickGame class implementation
QuickGame::QuickGame(QObject* parent) 
    : QObject(parent), currentBowlerIndex(0), gameActive(false), isHeld(false), 
      machineEnabled(true), timeLimit(0), gameLimit(0), gamesPlayed(0) {
    
    machine = new MachineInterface(this);
    
    connect(machine, &MachineInterface::ballDetected, this, &QuickGame::onBallDetected);
    connect(machine, &MachineInterface::machineError, this, &QuickGame::onMachineError);
    connect(machine, &MachineInterface::machineReady, this, &QuickGame::onMachineReady);
    
    gameTimer = new QTimer(this);
    gameTimer->setSingleShot(false);
    gameTimer->setInterval(60000); // 1 minute intervals
    connect(gameTimer, &QTimer::timeout, this, &QuickGame::onGameTimer);
}

QuickGame::~QuickGame() {
    stopMachineInterface();
}

void QuickGame::startGame(const QJsonObject& gameData) {
    qDebug() << "QuickGame::startGame called with data:" << gameData;
    
    // Parse game settings
    QJsonArray playersArray = gameData["players"].toArray();
    bowlers.clear();
    
    for (const QJsonValue& value : playersArray) {
        QString playerName = value.toString();
        if (!playerName.isEmpty()) {
            bowlers.append(Bowler(playerName));
            qDebug() << "Added player:" << playerName;
        }
    }
    
    if (bowlers.isEmpty()) {
        // Default players for testing
        bowlers.append(Bowler("Player 1"));
        bowlers.append(Bowler("Player 2"));
        qDebug() << "Added default players";
    }
    
    // Game limits
    timeLimit = gameData["time_limit"].toInt(0);
    gameLimit = gameData["game_limit"].toInt(0);
    gamesPlayed = 0;
    
    // Initialize game state
    currentBowlerIndex = 0;
    gameActive = true;
    isHeld = false;
    gameStartTime = QDateTime::currentMSecsSinceEpoch();
    
    qDebug() << "Game state initialized - emitting signals";
    
    // IMPORTANT: Emit signals in the right order
    emit gameStarted();
    emit currentPlayerChanged(getCurrentBowler().name, currentBowlerIndex);
    emit gameUpdated();
    
    // Start machine interface
    startMachineInterface();
    
    // Start game timer if time limit is set
    if (timeLimit > 0) {
        gameTimer->start();
    }
    
    qDebug() << "QuickGame::startGame completed successfully";
}

void QuickGame::resetGame() {
    qDebug() << "Resetting game";
    
    for (Bowler& bowler : bowlers) {
        bowler.reset();
    }
    
    currentBowlerIndex = 0;
    gamesPlayed = 0;
    
    // Reset machine
    if (machine && machine->isRunning()) {
        QJsonObject resetData;
        resetData["immediate"] = true;
        resetData["reset_type"] = "FULL_RESET";
        machine->sendCommand("machine_reset", resetData);
    }
    
    emit gameUpdated();
}

void QuickGame::endGame() {
    qDebug() << "Ending game";
    
    gameActive = false;
    gameTimer->stop();
    stopMachineInterface();
    
    // Prepare final results
    QJsonObject results;
    results["game_type"] = "quick_game";
    results["completion_time"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    results["total_time"] = (QDateTime::currentMSecsSinceEpoch() - gameStartTime) / 1000;
    
    QJsonArray finalScores;
    for (const Bowler& bowler : bowlers) {
        QJsonObject bowlerResult;
        bowlerResult["name"] = bowler.name;
        bowlerResult["final_score"] = bowler.totalScore;
        bowlerResult["frames_completed"] = bowler.currentFrame + (bowler.getCurrentFrame().isComplete ? 1 : 0);
        finalScores.append(bowlerResult);
    }
    results["final_scores"] = finalScores;
    
    emit gameEnded(results);
}

void QuickGame::addPlayer(const QString& playerName) {
    if (bowlers.size() < MAX_PLAYERS && !playerName.isEmpty()) {
        bowlers.append(Bowler(playerName));
        emit playerAdded(playerName);
        emit gameUpdated();
    }
}

void QuickGame::removePlayer(const QString& playerName) {
    for (int i = 0; i < bowlers.size(); ++i) {
        if (bowlers[i].name == playerName) {
            bowlers.removeAt(i);
            
            // Adjust current bowler index
            if (currentBowlerIndex >= i && currentBowlerIndex > 0) {
                currentBowlerIndex--;
            }
            if (currentBowlerIndex >= bowlers.size() && !bowlers.isEmpty()) {
                currentBowlerIndex = 0;
            }
            
            emit playerRemoved(playerName);
            emit gameUpdated();
            break;
        }
    }
}

void QuickGame::processBall(const QVector<int>& pins) {
    if (!gameActive || isHeld || bowlers.isEmpty()) {
        qDebug() << "Ball ignored - game not active, held, or no bowlers";
        return;
    }
    
    qDebug() << "Processing ball with pins:" << pins;
    
    Bowler& currentBowler = bowlers[currentBowlerIndex];
    Frame& currentFrame = currentBowler.getCurrentFrame();
    
    // Create ball object
    Ball newBall(pins);
    currentFrame.balls.append(newBall);
    
    // Check for special effects
    if (currentFrame.balls.size() == 1 && newBall.value == 15) {
        QJsonObject effectData;
        effectData["bowler"] = currentBowler.name;
        effectData["frame"] = currentBowler.currentFrame + 1;
        emit specialEffect("strike", effectData);
    } else if (currentFrame.balls.size() >= 2 && !currentFrame.isStrike() && currentFrame.isSpare()) {
        QJsonObject effectData;
        effectData["bowler"] = currentBowler.name;
        effectData["frame"] = currentBowler.currentFrame + 1;
        emit specialEffect("spare", effectData);
    }
    
    // Send ball data to server
    QJsonObject ballData;
    ballData["bowler"] = currentBowler.name;
    ballData["frame"] = currentBowler.currentFrame + 1;
    ballData["ball"] = currentFrame.balls.size();
    ballData["pins"] = QJsonArray::fromVariantList(QVariantList(pins.begin(), pins.end()));
    ballData["value"] = newBall.value;
    ballData["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    emit ballProcessed(ballData);
    
    // Update scoring and check completion
    updateScoring();
    checkFrameCompletion();
    
    emit gameUpdated();
}

void QuickGame::holdGame() {
    isHeld = !isHeld;
    
    if (machine && machine->isRunning()) {
        QJsonObject holdData;
        holdData["held"] = isHeld;
        machine->sendCommand("hold", holdData);
    }
    
    emit gameHeld(isHeld);
    qDebug() << "Game" << (isHeld ? "held" : "resumed");
}

void QuickGame::skipPlayer() {
    if (!gameActive || bowlers.isEmpty()) return;
    
    qDebug() << "Skipping player:" << getCurrentBowler().name;
    
    Bowler& currentBowler = bowlers[currentBowlerIndex];
    Frame& currentFrame = currentBowler.getCurrentFrame();
    
    // Fill remaining balls with misses
    while (currentFrame.balls.size() < 3 && !currentFrame.shouldComplete(currentBowler.currentFrame)) {
        currentFrame.balls.append(Ball());
    }
    
    currentFrame.isComplete = true;
    
    updateScoring();
    nextPlayer();
    
    emit gameUpdated();
}

void QuickGame::updateScore(const QJsonObject& scoreData) {
    QString bowlerName = scoreData["bowler"].toString();
    int frameIndex = scoreData["frame"].toInt() - 1; // Convert to 0-based
    
    for (Bowler& bowler : bowlers) {
        if (bowler.name == bowlerName && frameIndex >= 0 && frameIndex < 10) {
            // Update specific frame - implementation depends on score data format
            updateScoring();
            emit scoreUpdated(bowlers.indexOf(bowler));
            break;
        }
    }
}

void QuickGame::recalculateScores() {
    updateScoring();
    emit gameUpdated();
}

const Bowler& QuickGame::getCurrentBowler() const {
    if (currentBowlerIndex >= 0 && currentBowlerIndex < bowlers.size()) {
        return bowlers[currentBowlerIndex];
    }
    static Bowler emptyBowler;
    return emptyBowler;
}

Bowler& QuickGame::getCurrentBowler() {
    if (currentBowlerIndex >= 0 && currentBowlerIndex < bowlers.size()) {
        return bowlers[currentBowlerIndex];
    }
    static Bowler emptyBowler;
    return emptyBowler;
}

bool QuickGame::isGameComplete() const {
    if (bowlers.isEmpty()) return false;
    
    for (const Bowler& bowler : bowlers) {
        if (!bowler.isComplete()) return false;
    }
    return true;
}

int QuickGame::getCurrentFrame() const {
    return getCurrentBowler().currentFrame + 1;
}

int QuickGame::getCurrentBall() const {
    return getCurrentBowler().getCurrentFrame().balls.size() + 1;
}

QVector<int> QuickGame::getCurrentPinStates() const {
    const Frame& currentFrame = getCurrentBowler().getCurrentFrame();
    QVector<int> pinStates(5, 1); // All pins up initially
    
    // Apply pin knockdowns from previous balls in this frame
    for (const Ball& ball : currentFrame.balls) {
        for (int i = 0; i < 5 && i < ball.pins.size(); ++i) {
            if (ball.pins[i] == 1) {
                pinStates[i] = 0; // Pin down
            }
        }
    }
    
    return pinStates;
}

QJsonObject QuickGame::getGameState() const {
    QJsonObject state;
    state["game_active"] = gameActive;
    state["is_held"] = isHeld;
    state["current_bowler_index"] = currentBowlerIndex;
    state["time_limit"] = timeLimit;
    state["game_limit"] = gameLimit;
    state["games_played"] = gamesPlayed;
    state["game_start_time"] = gameStartTime;
    
    QJsonArray bowlersArray;
    for (const Bowler& bowler : bowlers) {
        bowlersArray.append(bowler.toJson());
    }
    state["bowlers"] = bowlersArray;
    
    return state;
}

void QuickGame::loadGameState(const QJsonObject& state) {
    gameActive = state["game_active"].toBool();
    isHeld = state["is_held"].toBool();
    currentBowlerIndex = state["current_bowler_index"].toInt();
    timeLimit = state["time_limit"].toInt();
    gameLimit = state["game_limit"].toInt();
    gamesPlayed = state["games_played"].toInt();
    gameStartTime = state["game_start_time"].toVariant().toLongLong();
    
    bowlers.clear();
    QJsonArray bowlersArray = state["bowlers"].toArray();
    for (const QJsonValue& value : bowlersArray) {
        Bowler bowler;
        bowler.fromJson(value.toObject());
        bowlers.append(bowler);
    }
    
    emit gameUpdated();
}

void QuickGame::setTimeLimit(int minutes) {
    timeLimit = minutes;
}

void QuickGame::setGameLimit(int games) {
    gameLimit = games;
}

QJsonObject QuickGame::getGameStatistics() const {
    QJsonObject stats;
    
    if (bowlers.isEmpty()) return stats;
    
    // Calculate statistics
    int totalScore = 0;
    int highScore = 0;
    int strikes = 0;
    int spares = 0;
    
    for (const Bowler& bowler : bowlers) {
        totalScore += bowler.totalScore;
        if (bowler.totalScore > highScore) {
            highScore = bowler.totalScore;
        }
        
        for (const Frame& frame : bowler.frames) {
            if (frame.isStrike()) strikes++;
            else if (frame.isSpare()) spares++;
        }
    }
    
    stats["total_score"] = totalScore;
    stats["average_score"] = totalScore / bowlers.size();
    stats["high_score"] = highScore;
    stats["total_strikes"] = strikes;
    stats["total_spares"] = spares;
    stats["players"] = bowlers.size();
    
    return stats;
}

void QuickGame::onBallDetected(const QVector<int>& pins) {
    processBall(pins);
}

void QuickGame::onMachineError(const QString& error) {
    qWarning() << "Machine error:" << error;
    emit errorOccurred(error);
}

void QuickGame::onMachineReady() {
    qDebug() << "Machine interface ready";
}

void QuickGame::onGameTimer() {
    // Check time limit
    if (timeLimit > 0) {
        qint64 elapsed = (QDateTime::currentMSecsSinceEpoch() - gameStartTime) / 60000; // minutes
        if (elapsed >= timeLimit) {
            endGame();
            return;
        }
    }
    
    // Other periodic checks can go here
}

void QuickGame::updateScoring() {
    for (Bowler& bowler : bowlers) {
        calculateBowlerScore(bowler);
    }
}

void QuickGame::calculateBowlerScore(Bowler& bowler) {
    int runningTotal = 0;
    
    for (int frameIdx = 0; frameIdx < bowler.frames.size(); ++frameIdx) {
        Frame& frame = bowler.frames[frameIdx];
        
        if (frame.balls.isEmpty()) {
            frame.frameScore = 0;
            frame.totalScore = runningTotal;
            continue;
        }
        
        int frameScore = calculateFrameScore(frame, frameIdx, bowler.frames);
        frame.frameScore = frameScore;
        runningTotal += frameScore;
        frame.totalScore = runningTotal;
    }
    
    bowler.totalScore = runningTotal;
}

int QuickGame::calculateFrameScore(const Frame& frame, int frameIndex, const QVector<Frame>& allFrames) {
    if (frameIndex < 9) { // Frames 1-9
        if (frame.isStrike()) {
            return 15 + getStrikeBonus(frameIndex, allFrames);
        } else if (frame.isSpare()) {
            return 15 + getSpareBonus(frameIndex, allFrames);
        } else {
            return frame.getFrameTotal();
        }
    } else { // Frame 10
        return frame.getFrameTotal();
    }
}

int QuickGame::getStrikeBonus(int frameIndex, const QVector<Frame>& frames) {
    if (frameIndex >= 8) return 0; // No bonus for frames 9-10
    
    int bonus = 0;
    int ballsNeeded = 2;
    
    // Get next two balls
    for (int nextFrameIdx = frameIndex + 1; nextFrameIdx < frames.size() && ballsNeeded > 0; ++nextFrameIdx) {
        const Frame& nextFrame = frames[nextFrameIdx];
        
        for (const Ball& ball : nextFrame.balls) {
            if (ballsNeeded > 0) {
                bonus += ball.value;
                ballsNeeded--;
            }
        }
    }
    
    return bonus;
}

int QuickGame::getSpareBonus(int frameIndex, const QVector<Frame>& frames) {
    if (frameIndex >= 9) return 0; // No bonus for frame 10
    
    // Get next ball
    if (frameIndex + 1 < frames.size()) {
        const Frame& nextFrame = frames[frameIndex + 1];
        if (!nextFrame.balls.isEmpty()) {
            return nextFrame.balls[0].value;
        }
    }
    
    return 0;
}

void QuickGame::checkFrameCompletion() {
    Bowler& currentBowler = bowlers[currentBowlerIndex];
    Frame& currentFrame = currentBowler.getCurrentFrame();
    
    if (currentFrame.shouldComplete(currentBowler.currentFrame)) {
        currentFrame.isComplete = true;
        emit frameCompleted(currentBowlerIndex, currentBowler.currentFrame);
        nextPlayer();
    }
}

void QuickGame::nextPlayer() {
    Bowler& currentBowler = bowlers[currentBowlerIndex];
    
    // Move to next frame if current frame is complete
    if (currentBowler.getCurrentFrame().isComplete && currentBowler.currentFrame < 9) {
        currentBowler.nextFrame();
    }
    
    // Move to next bowler
    currentBowlerIndex = (currentBowlerIndex + 1) % bowlers.size();
    
    // Check if all bowlers completed current round
    checkGameCompletion();
    
    emit currentPlayerChanged(getCurrentBowler().name, currentBowlerIndex);
}

void QuickGame::checkGameCompletion() {
    if (isGameComplete()) {
        gamesPlayed++;
        
        // Check if we should start another game
        if (gameLimit > 0 && gamesPlayed >= gameLimit) {
            endGame();
        } else {
            emit gameCompleted();
            // Could automatically start next game or wait for user input
        }
    }
}

void QuickGame::triggerSpecialEffect(const QString& effect, const QJsonObject& data) {
    emit specialEffect(effect, data);
}

void QuickGame::checkForSpecialEvents(const Ball& ball, const Frame& frame) {
    // Check for achievements or special conditions
    if (ball.value == 15 && frame.balls.size() == 1) {
        // Strike
        QJsonObject data;
        data["type"] = "strike";
        data["ball_value"] = ball.value;
        triggerSpecialEffect("strike", data);
    }
}

void QuickGame::startMachineInterface() {
    if (machineEnabled && machine) {
        machine->startDetection();
    }
}

void QuickGame::stopMachineInterface() {
    if (machine) {
        machine->stopDetection();
    }
}

void QuickGame::sendMachineCommand(const QString& command, const QJsonObject& data) {
    if (machine && machine->isRunning()) {
        machine->sendCommand(command, data);
    }
}

bool QuickGame::validateGameState() const {
    if (bowlers.isEmpty()) return false;
    if (currentBowlerIndex < 0 || currentBowlerIndex >= bowlers.size()) return false;
    
    for (const Bowler& bowler : bowlers) {
        if (!validateBowlerData(bowler)) return false;
    }
    
    return true;
}

bool QuickGame::validateBowlerData(const Bowler& bowler) const {
    if (bowler.frames.size() != 10) return false;
    if (bowler.currentFrame < 0 || bowler.currentFrame > 9) return false;
    
    for (int i = 0; i < bowler.frames.size(); ++i) {
        const Frame& frame = bowler.frames[i];
        
        // Validate ball count
        if (frame.balls.size() > 3) return false;
        
        // Validate pin values
        for (const Ball& ball : frame.balls) {
            if (ball.pins.size() != 5) return false;
            for (int pin : ball.pins) {
                if (pin < 0 || pin > 1) return false;
            }
            if (ball.value < 0 || ball.value > 15) return false;
        }
    }
    
    return true;
}