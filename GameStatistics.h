// GameStatistics.h - Track scores and achievements

#include <QObject>
#include <QVector>
#include <QDateTime>
#include <QMap>
#include <QString>

class Ball; 
class Bowler;

class GameStatistics : public QObject {
    Q_OBJECT
    
public:
    struct HighScoreRecord {
        QString bowlerName;
        int score;
        QString gameType;
        QDateTime dateTime;
        int gameNumber;
    };
    
    struct StrikeRecord {
        QString bowlerName;
        int consecutiveStrikes;
        QVector<int> frames;  // Which frames had strikes
        QString gameType;
        QDateTime dateTime;
        int gameNumber;
    };
    
    explicit GameStatistics(QObject* parent = nullptr);
    
    // Record tracking
    void recordGameCompletion(const QVector<Bowler>& bowlers, const QString& gameType, int gameNumber);
    void recordBallThrown(const QString& bowlerName, int frame, const Ball& ball, bool isStrike, bool isSpare);
    
    // Statistics queries
    QVector<HighScoreRecord> getTopScores(int limit = 10) const;
    QVector<StrikeRecord> getTopStrikeRecords(int limit = 10) const;
    QVector<HighScoreRecord> getRecentHighScores(int days = 30) const;
    
    // Save/load
    void saveStatistics();
    void loadStatistics();
    
signals:
    void newHighScore(const HighScoreRecord& record);
    void newStrikeRecord(const StrikeRecord& record);
    
private:
    void updateStrikeTracking(const QString& bowlerName, int frame, bool isStrike, int gameNumber);
    bool isNewHighScore(int score) const;
    bool isNewStrikeRecord(int consecutiveStrikes) const;
    
    QVector<HighScoreRecord> highScores;
    QVector<StrikeRecord> strikeRecords;
    QMap<QString, QVector<int>> currentStrikeSequences; // bowlerName -> frame numbers with strikes
    QString statisticsFilePath;
};

