// GameStatistics.cpp

#include <QWidget>
#include <QFrame>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QScrollArea>
#include <QPushButton>
#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>
#include <QTimer>
#include <QFont>
#include <QPalette>
#include <QPixmap>
#include <QPainter>
#include <QStyleOption>
#include <QVector>
#include <QJsonObject>

// GameStatistics.cpp implementation
GameStatistics::GameStatistics(QObject* parent) : QObject(parent) {
    statisticsFilePath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/game_statistics.json";
    QDir().mkpath(QFileInfo(statisticsFilePath).path());
    loadStatistics();
}

void GameStatistics::recordGameCompletion(const QVector<Bowler>& bowlers, const QString& gameType, int gameNumber) {
    qDebug() << "Recording game completion statistics";
    
    QDateTime now = QDateTime::currentDateTime();
    
    for (const Bowler& bowler : bowlers) {
        // Record high score if applicable
        if (isNewHighScore(bowler.totalScore)) {
            HighScoreRecord record;
            record.bowlerName = bowler.name;
            record.score = bowler.totalScore;
            record.gameType = gameType;
            record.dateTime = now;
            record.gameNumber = gameNumber;
            
            highScores.append(record);
            
            // Sort and keep top 100
            std::sort(highScores.begin(), highScores.end(), 
                     [](const HighScoreRecord& a, const HighScoreRecord& b) {
                         return a.score > b.score;
                     });
            
            if (highScores.size() > 100) {
                highScores.resize(100);
            }
            
            qDebug() << "New high score recorded:" << bowler.name << bowler.totalScore;
            emit newHighScore(record);
        }
        
        // Process strike sequences
        QVector<int> strikeFrames;
        for (int i = 0; i < bowler.frames.size(); ++i) {
            if (bowler.frames[i].isStrike()) {
                strikeFrames.append(i + 1); // 1-based frame numbers
            }
        }
        
        if (!strikeFrames.isEmpty()) {
            // Find consecutive sequences
            QVector<int> currentSequence;
            int maxConsecutive = 0;
            
            for (int i = 0; i < strikeFrames.size(); ++i) {
                if (currentSequence.isEmpty() || strikeFrames[i] == currentSequence.last() + 1) {
                    currentSequence.append(strikeFrames[i]);
                } else {
                    // End of sequence
                    maxConsecutive = qMax(maxConsecutive, currentSequence.size());
                    currentSequence.clear();
                    currentSequence.append(strikeFrames[i]);
                }
            }
            maxConsecutive = qMax(maxConsecutive, currentSequence.size());
            
            // Record if it's a new record
            if (isNewStrikeRecord(maxConsecutive)) {
                StrikeRecord record;
                record.bowlerName = bowler.name;
                record.consecutiveStrikes = maxConsecutive;
                record.frames = strikeFrames;
                record.gameType = gameType;
                record.dateTime = now;
                record.gameNumber = gameNumber;
                
                strikeRecords.append(record);
                
                // Sort and keep top 50
                std::sort(strikeRecords.begin(), strikeRecords.end(),
                         [](const StrikeRecord& a, const StrikeRecord& b) {
                             return a.consecutiveStrikes > b.consecutiveStrikes;
                         });
                
                if (strikeRecords.size() > 50) {
                    strikeRecords.resize(50);
                }
                
                qDebug() << "New strike record:" << bowler.name << maxConsecutive << "consecutive strikes";
                emit newStrikeRecord(record);
            }
        }
    }
    
    // Clear current strike tracking
    currentStrikeSequences.clear();
    
    // Save updated statistics
    saveStatistics();
}

void GameStatistics::recordBallThrown(const QString& bowlerName, int frame, const Ball& ball, bool isStrike, bool isSpare) {
    Q_UNUSED(ball)
    Q_UNUSED(isSpare)
    
    if (isStrike) {
        if (!currentStrikeSequences.contains(bowlerName)) {
            currentStrikeSequences[bowlerName] = QVector<int>();
        }
        currentStrikeSequences[bowlerName].append(frame);
        
        qDebug() << bowlerName << "strike in frame" << frame 
                 << "- sequence length:" << currentStrikeSequences[bowlerName].size();
    } else {
        // End any current strike sequence
        if (currentStrikeSequences.contains(bowlerName)) {
            qDebug() << bowlerName << "strike sequence ended at" << currentStrikeSequences[bowlerName].size();
        }
    }
}

bool GameStatistics::isNewHighScore(int score) const {
    if (highScores.isEmpty()) return true;
    
    // Check if it's in top 100 or better than lowest recorded score
    if (highScores.size() < 100) return true;
    
    return score > highScores.last().score;
}

bool GameStatistics::isNewStrikeRecord(int consecutiveStrikes) const {
    if (strikeRecords.isEmpty()) return consecutiveStrikes >= 3; // Only record 3+ consecutive strikes
    
    // Check if it's in top 50 or better than lowest recorded
    if (strikeRecords.size() < 50) return consecutiveStrikes >= 3;
    
    return consecutiveStrikes > strikeRecords.last().consecutiveStrikes;
}

void GameStatistics::saveStatistics() {
    QJsonObject data;
    
    // Save high scores
    QJsonArray scoresArray;
    for (const HighScoreRecord& record : highScores) {
        QJsonObject scoreObj;
        scoreObj["bowler_name"] = record.bowlerName;
        scoreObj["score"] = record.score;
        scoreObj["game_type"] = record.gameType;
        scoreObj["date_time"] = record.dateTime.toString(Qt::ISODate);
        scoreObj["game_number"] = record.gameNumber;
        scoresArray.append(scoreObj);
    }
    data["high_scores"] = scoresArray;
    
    // Save strike records
    QJsonArray strikesArray;
    for (const StrikeRecord& record : strikeRecords) {
        QJsonObject strikeObj;
        strikeObj["bowler_name"] = record.bowlerName;
        strikeObj["consecutive_strikes"] = record.consecutiveStrikes;
        
        QJsonArray framesArray;
        for (int frame : record.frames) {
            framesArray.append(frame);
        }
        strikeObj["frames"] = framesArray;
        strikeObj["game_type"] = record.gameType;
        strikeObj["date_time"] = record.dateTime.toString(Qt::ISODate);
        strikeObj["game_number"] = record.gameNumber;
        strikesArray.append(strikeObj);
    }
    data["strike_records"] = strikesArray;
    
    // Write to file
    QFile file(statisticsFilePath);
    if (file.open(QIODevice::WriteOnly)) {
        QJsonDocument doc(data);
        file.write(doc.toJson());
        file.close();
        qDebug() << "Statistics saved to" << statisticsFilePath;
    }
}

void GameStatistics::loadStatistics() {
    QFile file(statisticsFilePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "No statistics file found, starting fresh";
        return;
    }
    
    QByteArray data = file.readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    QJsonObject root = doc.object();
    
    // Load high scores
    highScores.clear();
    QJsonArray scoresArray = root["high_scores"].toArray();
    for (const QJsonValue& value : scoresArray) {
        QJsonObject scoreObj = value.toObject();
        HighScoreRecord record;
        record.bowlerName = scoreObj["bowler_name"].toString();
        record.score = scoreObj["score"].toInt();
        record.gameType = scoreObj["game_type"].toString();
        record.dateTime = QDateTime::fromString(scoreObj["date_time"].toString(), Qt::ISODate);
        record.gameNumber = scoreObj["game_number"].toInt();
        highScores.append(record);
    }
    
    // Load strike records
    strikeRecords.clear();
    QJsonArray strikesArray = root["strike_records"].toArray();
    for (const QJsonValue& value : strikesArray) {
        QJsonObject strikeObj = value.toObject();
        StrikeRecord record;
        record.bowlerName = strikeObj["bowler_name"].toString();
        record.consecutiveStrikes = strikeObj["consecutive_strikes"].toInt();
        
        QJsonArray framesArray = strikeObj["frames"].toArray();
        for (const QJsonValue& frameValue : framesArray) {
            record.frames.append(frameValue.toInt());
        }
        record.gameType = strikeObj["game_type"].toString();
        record.dateTime = QDateTime::fromString(strikeObj["date_time"].toString(), Qt::ISODate);
        record.gameNumber = strikeObj["game_number"].toInt();
        strikeRecords.append(record);
    }
    
    qDebug() << "Statistics loaded:" << highScores.size() << "high scores," << strikeRecords.size() << "strike records";
}