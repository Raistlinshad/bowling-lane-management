// ThreeSixNineTracker.h - Separate system for 3-6-9 tracking
#ifndef THREESIXNINETRACKER_H
#define THREESIXNINETRACKER_H

#include <QObject>
#include <QString>
#include <QVector>
#include <QMap>

class ThreeSixNineTracker : public QObject {
    Q_OBJECT
    
public:
    enum class ParticipationMode {
        Everyone,      // All bowlers participate
        Selectable     // Can be enabled/disabled per bowler before 2nd frame of 1st game
    };
    
    struct ParticipantStatus {
        QString bowlerName;
        bool participating = false;
        int strikesAchieved = 0;
        QVector<int> targetFrames;
        QVector<bool> frameResults; // true if strike achieved in target frame
        QString currentStatus;       // "Active", "Winner", "6 of 7 Congrats", "Eliminated"
        int dotsRemaining = 2;
    };
    
    explicit ThreeSixNineTracker(QObject* parent = nullptr);
    
    // Setup
    void initialize(const QVector<QString>& bowlerNames, const QVector<int>& strikeFrames, ParticipationMode mode);
    void setBowlerParticipation(const QString& bowlerName, bool participating);
    bool canToggleParticipation() const; // Only before 2nd frame of 1st game
    
    // Game tracking
    void recordFrameResult(const QString& bowlerName, int gameNumber, int frameNumber, bool isStrike);
    void startNewGame(int gameNumber);
    
    // Status queries
    ParticipantStatus getBowlerStatus(const QString& bowlerName) const;
    QVector<ParticipantStatus> getAllStatuses() const;
    bool isActive() const { return enabled; }
    
    // Display info
    QString getStatusText(const QString& bowlerName) const;
    int getDotsCount(const QString& bowlerName) const;
    
signals:
    void participantWon(const QString& bowlerName);
    void participantAlmostWon(const QString& bowlerName); // 6 of 7
    void participantEliminated(const QString& bowlerName);
    void statusChanged(const QString& bowlerName, const QString& status);
    
private:
    void updateParticipantStatus(const QString& bowlerName);
    bool isTargetFrame(int gameNumber, int frameNumber) const;
    void checkForCompletion(const QString& bowlerName);
    
    bool enabled;
    ParticipationMode mode;
    QVector<int> targetFrames;      // Which frames across all games need strikes
    QMap<QString, ParticipantStatus> participants;
    int currentGameNumber;
    int totalTargetFrames;          // Usually 7 (3+3+1 or similar pattern)
    bool participationLocked;       // After 2nd frame of 1st game
};


#endif // THREESIXNINETRACKER_H
