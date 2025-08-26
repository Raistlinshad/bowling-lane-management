// ThreeSixNineTracker.cpp

#include "ThreeSixNineTracker.h"

#include <QDebug>
#include <QTimer>

// Three Six Nine Tracking
ThreeSixNineTracker::ThreeSixNineTracker(QObject* parent) 
    : QObject(parent), enabled(false), mode(ParticipationMode::Everyone),
      currentGameNumber(1), totalTargetFrames(7), participationLocked(false) {
}

void ThreeSixNineTracker::initialize(const QVector<QString>& bowlerNames, 
                                    const QVector<int>& strikeFrames, 
                                    ParticipationMode mode) {
    this->mode = mode;
    this->targetFrames = strikeFrames;
    this->totalTargetFrames = strikeFrames.size();
    this->enabled = true;
    this->participationLocked = false;
    this->currentGameNumber = 1;
    
    participants.clear();
    
    for (const QString& bowlerName : bowlerNames) {
        ParticipantStatus status;
        status.bowlerName = bowlerName;
        status.participating = (mode == ParticipationMode::Everyone);
        status.targetFrames = strikeFrames;
        status.frameResults.resize(totalTargetFrames, false);
        status.currentStatus = status.participating ? "Active" : "Not Participating";
        status.dotsRemaining = 2;
        status.strikesAchieved = 0;
        
        participants[bowlerName] = status;
    }
    
    qDebug() << "3-6-9 initialized for" << bowlerNames.size() << "bowlers with" 
             << totalTargetFrames << "target frames";
}

void ThreeSixNineTracker::recordFrameResult(const QString& bowlerName, int gameNumber, 
                                           int frameNumber, bool isStrike) {
    if (!enabled || !participants.contains(bowlerName)) return;
    
    ParticipantStatus& status = participants[bowlerName];
    if (!status.participating || status.currentStatus == "Winner" || status.currentStatus == "Eliminated") {
        return;
    }
    
    // Check if this is a target frame
    int absoluteFrame = (gameNumber - 1) * 10 + frameNumber; // Convert to absolute frame number
    
    for (int i = 0; i < targetFrames.size(); ++i) {
        if (targetFrames[i] == absoluteFrame) {
            qDebug() << bowlerName << "bowled in target frame" << absoluteFrame << "- strike:" << isStrike;
            
            status.frameResults[i] = isStrike;
            
            if (isStrike) {
                status.strikesAchieved++;
                qDebug() << bowlerName << "achieved strike" << status.strikesAchieved << "of" << totalTargetFrames;
            } else {
                // Missed a target frame - lose a dot
                status.dotsRemaining = qMax(0, status.dotsRemaining - 1);
                qDebug() << bowlerName << "missed target frame - dots remaining:" << status.dotsRemaining;
                
                if (status.dotsRemaining == 0) {
                    status.currentStatus = "Eliminated";
                    emit participantEliminated(bowlerName);
                }
            }
            
            updateParticipantStatus(bowlerName);
            checkForCompletion(bowlerName);
            break;
        }
    }
    
    // Lock participation after 2nd frame of 1st game
    if (gameNumber == 1 && frameNumber == 2 && !participationLocked) {
        participationLocked = true;
        qDebug() << "3-6-9 participation locked after 2nd frame of 1st game";
    }
}

void ThreeSixNineTracker::checkForCompletion(const QString& bowlerName) {
    ParticipantStatus& status = participants[bowlerName];
    
    int completedTargets = 0;
    for (bool result : status.frameResults) {
        if (result) completedTargets++;
    }
    
    if (completedTargets == totalTargetFrames) {
        status.currentStatus = "Winner";
        emit participantWon(bowlerName);
        qDebug() << bowlerName << "won 3-6-9 with all" << totalTargetFrames << "strikes!";
    } else if (completedTargets == totalTargetFrames - 1) {
        // Check if all target frames have been attempted
        bool allAttempted = true;
        for (int i = 0; i < targetFrames.size(); ++i) {
            int absoluteFrame = targetFrames[i];
            int gameNum = (absoluteFrame - 1) / 10 + 1;
            int frameNum = (absoluteFrame - 1) % 10 + 1;
            
            // If this frame is in a future game, not all attempted yet
            if (gameNum > currentGameNumber) {
                allAttempted = false;
                break;
            }
        }
        
        if (allAttempted && status.currentStatus != "Winner") {
            status.currentStatus = "6 of 7 Congrats";
            emit participantAlmostWon(bowlerName);
            qDebug() << bowlerName << "achieved 6 of 7 in 3-6-9!";
        }
    }
}

void ThreeSixNineTracker::setBowlerParticipation(const QString& bowlerName, bool participating) {
    if (!canToggleParticipation() || !participants.contains(bowlerName)) return;
    
    participants[bowlerName].participating = participating;
    participants[bowlerName].currentStatus = participating ? "Active" : "Not Participating";
    
    if (participating) {
        participants[bowlerName].dotsRemaining = 2;
    }
    
    qDebug() << bowlerName << "3-6-9 participation set to:" << participating;
    emit statusChanged(bowlerName, participants[bowlerName].currentStatus);
}

bool ThreeSixNineTracker::canToggleParticipation() const {
    return mode == ParticipationMode::Selectable && !participationLocked;
}

QString ThreeSixNineTracker::getStatusText(const QString& bowlerName) const {
    if (!participants.contains(bowlerName)) return "";
    
    const ParticipantStatus& status = participants.value(bowlerName);
    
    if (status.currentStatus == "Winner") {
        return "3-6-9 WINNER!";
    } else if (status.currentStatus == "6 of 7 Congrats") {
        return "6 of 7 Congrats";
    } else if (status.currentStatus == "Eliminated") {
        return "";  // Don't show anything for eliminated
    } else if (status.participating) {
        return "";  // Show dots instead of text for active participants
    }
    
    return "";
}

int ThreeSixNineTracker::getDotsCount(const QString& bowlerName) const {
    if (!participants.contains(bowlerName)) return 0;
    
    const ParticipantStatus& status = participants.value(bowlerName);
    
    if (status.participating && status.currentStatus == "Active") {
        return status.dotsRemaining;
    }
    
    return 0; // No dots for non-participating, eliminated, or winners
}

ThreeSixNineTracker::ParticipantStatus ThreeSixNineTracker::getBowlerStatus(const QString& bowlerName) const {
    return participants.value(bowlerName, ParticipantStatus());
}

QVector<ThreeSixNineTracker::ParticipantStatus> ThreeSixNineTracker::getAllStatuses() const {
    QVector<ParticipantStatus> statuses;
    for (auto it = participants.begin(); it != participants.end(); ++it) {
        statuses.append(it.value());
    }
    return statuses;
}