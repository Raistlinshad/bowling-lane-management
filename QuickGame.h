#ifndef QUICKGAME_H
#define QUICKGAME_H

#include <QObject>
#include <QVector>
#include <QJsonObject>
#include <QJsonArray>
#include <QProcess>
#include <QTimer>
#include <QDebug>

// Forward declarations
class Ball;
class Frame;
class Bowler;
class MachineInterface;

// Ball class representing a single throw
class Ball {
public:
    Ball(const QVector<int>& pins = QVector<int>(5, 0), int value = 0);
    
    QVector<int> pins;  // [lTwo, lThree, cFive, rThree, rTwo] - 0=down, 1=up
    int value;          // Total pin value (Canadian 5-pin scoring)
    
    // Canadian 5-pin pin values: L2=2, L3=3, C5=5, R3=3, R2=2
    static int calculateValue(const QVector<int>& pins);
};

// Frame class representing a single frame (1-10)
class Frame {
public:
    Frame();
    
    QVector<Ball> balls;
    int totalScore;     // Running total through this frame
    int frameScore;     // Score for this frame only
    bool isComplete;
    
    // Canadian 5-pin specific methods
    bool isStrike() const;      // First ball = 15 points
    bool isSpare() const;       // Any two balls = 15 points
    bool isOpen() const;        // Less than 15 points total
    
    QString getDisplayText() const;
    
    // Frame completion logic
    bool shouldComplete(int frameNumber) const;
    int getBallCount() const { return balls.size(); }
    
    // Scoring helpers
    int getFrameTotal() const;
    bool needsBonus() const;
    int getBonusBalls() const;  // How many bonus balls needed (1 for spare, 2 for strike)
};

// Bowler class representing a player
class Bowler {
public:
    Bowler(const QString& name = "");
    
    QString name;
    QVector<Frame> frames;
    int currentFrame;
    int totalScore;
    
    bool isComplete() const;
    Frame& getCurrentFrame();
    const Frame& getCurrentFrame() const;

    bool operator==(const Bowler& other) const {
    return name == other.name;
    }
    
    // Game state methods
    void nextFrame();
    void reset();
    
    // Serialization
    QJsonObject toJson() const;
    void fromJson(const QJsonObject& json);

};

// Machine interface for communicating with Python ball detector
class MachineInterface : public QObject {
    Q_OBJECT
    
public:
    explicit MachineInterface(QObject* parent = nullptr);
    ~MachineInterface();
    
    void startDetection();
    void stopDetection();
    
    bool isRunning() const;
    void sendCommand(const QString& command, const QJsonObject& data = QJsonObject());
    void machineReset();
};

signals:
    void ballDetected(const QVector<int>& pins);
    void machineError(const QString& error);
    void machineReady();
    void machineStatusChanged(const QString& status);

private slots:
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onDataReady();
    void onErrorOccurred(QProcess::ProcessError error);

private:
    void setupProcess();
    void processMachineOutput(const QString& line);
    
    QProcess* pythonProcess;
    QTimer* heartbeatTimer;
    bool machineIsReady;
    QString lastError;
};

// Main Quick Game class
class QuickGame : public QObject {
    Q_OBJECT
    
public:
    explicit QuickGame(QObject* parent = nullptr);
    ~QuickGame();
    
    // Game management
    void startGame(const QJsonObject& gameData);
    void resetGame();
    void endGame();
    
    // Player management
    void addPlayer(const QString& playerName);
    void removePlayer(const QString& playerName);
    void movePlayerToPosition(int from, int to);
    
    // Game flow control
    void processBall(const QVector<int>& pins);
    void holdGame();
    void skipPlayer();
    void skipFrame();

    static const QVector<int> PIN_VALUES;  // {2, 3, 5, 3, 2}
    
    // Score management
    void updateScore(const QJsonObject& scoreData);
    void recalculateScores();
    
    // Game state queries
    const QVector<Bowler>& getBowlers() const { return bowlers; }
    int getCurrentBowlerIndex() const { return currentBowlerIndex; }
    const Bowler& getCurrentBowler() const;
    Bowler& getCurrentBowler();
    
    bool isGameActive() const { return gameActive; }
    bool isGameHeld() const { return isHeld; }
    bool isGameComplete() const;
    
    // Current game state
    int getCurrentFrame() const;
    int getCurrentBall() const;
    QVector<int> getCurrentPinStates() const;
    
    // Serialization for server communication
    QJsonObject getGameState() const;
    void loadGameState(const QJsonObject& state);
    
    // Settings
    void setTimeLimit(int minutes);
    void setGameLimit(int games);
    
    // Statistics
    QJsonObject getGameStatistics() const;

signals:
    void gameStarted();
    void gameEnded(const QJsonObject& results);
    void gameUpdated();
    void gameHeld(bool held);
    
    void currentPlayerChanged(const QString& playerName, int bowlerIndex);
    void frameCompleted(int bowlerIndex, int frameNumber);
    void gameCompleted();
    
    void specialEffect(const QString& effect, const QJsonObject& data = QJsonObject());
    void ballProcessed(const QJsonObject& ballData);
    
    void playerAdded(const QString& playerName);
    void playerRemoved(const QString& playerName);
    
    void scoreUpdated(int bowlerIndex);
    void errorOccurred(const QString& error);

private slots:
    void onBallDetected(const QVector<int>& pins);
    void onMachineError(const QString& error);
    void onMachineReady();
    void onGameTimer();

private:
    // Game logic
    void updateScoring();
    void checkFrameCompletion();
    void nextPlayer();
    void checkGameCompletion();
    
    // Scoring helpers
    void calculateBowlerScore(Bowler& bowler);
    int calculateFrameScore(const Frame& frame, int frameIndex, const QVector<Frame>& allFrames);
    int getStrikeBonus(int frameIndex, const QVector<Frame>& frames);
    int getSpareBonus(int frameIndex, const QVector<Frame>& frames);
    
    // Special effects
    void triggerSpecialEffect(const QString& effect, const QJsonObject& data = QJsonObject());
    void checkForSpecialEvents(const Ball& ball, const Frame& frame);
    
    // Machine communication
    void startMachineInterface();
    void stopMachineInterface();
    void sendMachineCommand(const QString& command, const QJsonObject& data = QJsonObject());
    
    // Game state validation
    bool validateGameState() const;
    bool validateBowlerData(const Bowler& bowler) const;
    
    // Data members
    QVector<Bowler> bowlers;
    int currentBowlerIndex;
    
    bool gameActive;
    bool isHeld;
    bool machineEnabled;
    
    // Game settings
    int timeLimit;      // Minutes (0 = no limit)
    int gameLimit;      // Number of games (0 = no limit)
    int gamesPlayed;
    
    // Timers
    QTimer* gameTimer;
    qint64 gameStartTime;
    
    // Machine interface
    MachineInterface* machine;
    
    // Special conditions tracking
    QVector<QString> activeEffects;
    
    // Constants
    static const int MAX_PLAYERS = 6;
    static const int FRAMES_PER_GAME = 10;
    static const int MAX_BALLS_PER_FRAME = 3;
    static const int PERFECT_SCORE = 450;  // 15 * 30 (10 strikes + 20 bonus balls)
    
    // Canadian 5-pin specific constants
    static const int TOTAL_PIN_VALUE = 15;
    static const int STRIKE_VALUE = 15;
    static const int SPARE_VALUE = 15;
};

#endif // QUICKGAME_H