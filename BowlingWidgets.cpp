#include "BowlingWidgets.h"
#include <QMouseEvent>
#include <QKeyEvent>
#include <QResizeEvent>
#include <QPaintEvent>
#include <QDebug>
#include <QEasingCurve>

// Static constants for PinDisplayWidget - CORRECTED LAYOUT
const QVector<QPointF> PinDisplayWidget::pinPositions = {
    QPointF(0.15, 0.3),   // lTwo (top-left)
    QPointF(0.35, 0.45),  // lThree (upper-left) 
    QPointF(0.5, 0.6),    // cFive (bottom-center)
    QPointF(0.65, 0.45),  // rThree (upper-right)
    QPointF(0.85, 0.3)    // rTwo (top-right)
};

const QStringList PinDisplayWidget::pinNames = {"L2", "L3", "C5", "R3", "R2"};
const QVector<int> PinDisplayWidget::pinValues = {2, 3, 5, 3, 2};

// PinDisplayWidget implementation
PinDisplayWidget::PinDisplayWidget(QWidget* parent) 
    : QWidget(parent), displayMode("large"), upColor("#87CEEB"), downColor("#2F4F4F"), 
      pinAnimation(nullptr), isAnimating(false), m_animationProgress(0.0) {
    
    pinStates.resize(5);
    resetPins();
    setupPinLayout();
    setMinimumSize(200, 150);
    
    // Dark theme styling
    setStyleSheet(R"(
        PinDisplayWidget {
            background-color: #1a1a1a;
            border: 2px solid #444444;
            border-radius: 10px;
        }
    )");
}

void PinDisplayWidget::setPinStates(const QVector<int>& states) {
    if (states.size() == 5) {
        pinStates = states;
        updatePinDisplay();
    }
}

void PinDisplayWidget::resetPins() {
    pinStates.fill(1, 5); // All pins up
    updatePinDisplay();
}

void PinDisplayWidget::animatePinFall(const QVector<int>& beforeStates, const QVector<int>& afterStates) {
    if (beforeStates.size() != 5 || afterStates.size() != 5) return;
    
    animationStartStates = beforeStates;
    animationEndStates = afterStates;
    
    if (!pinAnimation) {
        pinAnimation = new QPropertyAnimation(this, "animationProgress");
        connect(pinAnimation, &QPropertyAnimation::finished, this, &PinDisplayWidget::onAnimationFinished);
    }
    
    isAnimating = true;
    pinAnimation->setDuration(1200);
    pinAnimation->setEasingCurve(QEasingCurve::OutBounce);
    pinAnimation->setStartValue(0.0);
    pinAnimation->setEndValue(1.0);
    pinAnimation->start();
}

void PinDisplayWidget::setDisplayMode(const QString& mode) {
    displayMode = mode;
    
    if (mode == "large") {
        setMinimumSize(200, 150);
    } else if (mode == "small") {
        setMinimumSize(140, 105);
    } else if (mode == "mini") {
        setMinimumSize(100, 75);
    }
    
    update();
}

void PinDisplayWidget::setColorScheme(const QString& upColor, const QString& downColor) {
    this->upColor = upColor;
    this->downColor = downColor;
    updatePinDisplay();
}

void PinDisplayWidget::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event)
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // Draw background
    painter.fillRect(rect(), QColor("#1a1a1a"));
    
    // Draw pin positions guide (subtle)
    painter.setPen(QPen(QColor("#333333"), 1, Qt::DashLine));
    painter.setBrush(Qt::NoBrush);
    
    // Draw connecting lines to show pin arrangement
    QRect widgetRect = rect().adjusted(10, 10, -10, -10);
    
    // Get pin centers
    QVector<QPointF> centers;
    for (int i = 0; i < 5; ++i) {
        QRect pinRect = getPinRect(i);
        centers.append(pinRect.center());
    }
    
    // Draw formation lines (very subtle)
    if (displayMode != "mini") {
        painter.setPen(QPen(QColor("#222222"), 1));
        // L2 to R2 (top line)
        painter.drawLine(centers[0], centers[4]);
        // L3 to R3 (middle line) 
        painter.drawLine(centers[1], centers[3]);
        // Connect to center pin
        painter.drawLine(centers[1], centers[2]);
        painter.drawLine(centers[3], centers[2]);
    }
    
    // Draw pins
    for (int i = 0; i < 5; ++i) {
        QRect pinRect = getPinRect(i);
        bool isUp = pinStates[i] == 1;
        
        if (isAnimating) {
            // During animation, show falling effect
            qreal progress = m_animationProgress; // FIXED: Use member variable
            bool finalState = animationEndStates[i] == 1;
            
            if (animationStartStates[i] == 1 && animationEndStates[i] == 0) {
                // Pin falling - show tilted/fading effect
                painter.save();
                painter.translate(pinRect.center());
                painter.rotate(progress * 90); // Tilt as it falls
                painter.translate(-pinRect.center());
                drawPin(painter, i, pinRect, finalState, 1.0 - progress * 0.5);
                painter.restore();
            } else {
                drawPin(painter, i, pinRect, isUp);
            }
        } else {
            drawPin(painter, i, pinRect, isUp);
        }
    }
    
    // Draw value display
    if (displayMode == "large") {
        int totalValue = 0;
        for (int i = 0; i < 5; ++i) {
            if (pinStates[i] == 0) { // Pin down
                totalValue += pinValues[i];
            }
        }
        
        painter.setPen(QColor("#FFD700"));
        painter.setFont(QFont("Arial", 14, QFont::Bold));
        QString valueText = QString("Value: %1").arg(totalValue);
        painter.drawText(rect().adjusted(5, 5, -5, -5), Qt::AlignBottom | Qt::AlignRight, valueText);
    }
}

void PinDisplayWidget::resizeEvent(QResizeEvent* event) {
    Q_UNUSED(event)
    updatePinDisplay();
}

void PinDisplayWidget::onAnimationFinished() {
    isAnimating = false;
    pinStates = animationEndStates;
    update();
}

void PinDisplayWidget::setupPinLayout() {
    // Pin layout is handled in paintEvent
}

void PinDisplayWidget::updatePinDisplay() {
    update(); // Trigger a repaint
}

void PinDisplayWidget::drawPin(QPainter& painter, int pinIndex, const QRect& rect, bool isUp, qreal opacity) {
    painter.setOpacity(opacity);
    
    // Set pin colors with better contrast for dark theme
    QColor pinColor;
    if (isUp) {
        pinColor = QColor(upColor);
        if (pinColor == QColor("#87CEEB")) { // Default light blue
            pinColor = QColor("#4169E1"); // Royal blue - better for dark theme
        }
    } else {
        pinColor = QColor(downColor);
        if (pinColor == QColor("#2F4F4F")) { // Default dark slate gray
            pinColor = QColor("#696969"); // Dim gray - better contrast
        }
    }
    
    // Draw pin shadow first (for 3D effect)
    if (isUp) {
        QRect shadowRect = rect.adjusted(2, 2, 2, 2);
        painter.setBrush(QColor(0, 0, 0, 50));
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(shadowRect);
    }
    
    // Draw main pin body
    painter.setBrush(QBrush(pinColor));
    painter.setPen(QPen(Qt::white, 2));
    
    if (isUp) {
        // Standing pin - draw as circle with highlight
        painter.drawEllipse(rect);
        
        // Add highlight for 3D effect
        QRect highlightRect = QRect(rect.left() + rect.width()/4, 
                                  rect.top() + rect.height()/4,
                                  rect.width()/3, rect.height()/3);
        painter.setBrush(QColor(255, 255, 255, 80));
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(highlightRect);
    } else {
        // Fallen pin - draw as flattened ellipse
        painter.setBrush(QBrush(pinColor));
        painter.setPen(QPen(Qt::darkGray, 1));
        
        QRect flatRect = QRect(rect.left() - 5, rect.center().y() - 3, 
                              rect.width() + 10, 6);
        painter.drawEllipse(flatRect);
    }
    
    // Draw pin label
    painter.setOpacity(1.0);
    painter.setPen(Qt::white);
    QFont font = QFont("Arial", displayMode == "mini" ? 8 : (displayMode == "small" ? 10 : 12), QFont::Bold);
    painter.setFont(font);
    
    painter.drawText(rect, Qt::AlignCenter, pinNames[pinIndex]);
    
    // Draw pin value below (except for mini mode)
    if (displayMode != "mini") {
        QRect valueRect = rect.adjusted(0, rect.height() * 0.7, 0, rect.height() * 0.3);
        font.setPointSize(font.pointSize() - 2);
        painter.setFont(font);
        painter.setPen(QColor("#FFD700")); // Gold color for values
        painter.drawText(valueRect, Qt::AlignCenter, QString::number(pinValues[pinIndex]));
    }
}

QRect PinDisplayWidget::getPinRect(int pinIndex) const {
    if (pinIndex < 0 || pinIndex >= pinPositions.size()) {
        return QRect();
    }
    
    QPointF pos = pinPositions[pinIndex];
    QSize widgetSize = size();
    
    int pinSize = displayMode == "mini" ? 25 : (displayMode == "small" ? 35 : 45);
    
    int x = static_cast<int>(pos.x() * widgetSize.width() - pinSize / 2);
    int y = static_cast<int>(pos.y() * widgetSize.height() - pinSize / 2);
    
    return QRect(x, y, pinSize, pinSize);
}

// Enhanced BowlerWidget.cpp to match your frame layout specification:

class EnhancedBowlerWidget : public QFrame {
    Q_OBJECT
    
public:
    explicit EnhancedBowlerWidget(const Bowler& bowler, bool isCurrentPlayer = false, 
                                 const QJsonObject& displayOptions = QJsonObject(), QWidget* parent = nullptr)
        : QFrame(parent), bowlerData(bowler), isCurrentPlayer(isCurrentPlayer), displayOptions(displayOptions) {
        setupEnhancedUI();
        updateDisplay();
    }
    
private:
    void setupEnhancedUI() {
        setMinimumHeight(120);  // Taller for better visibility
        setFrameStyle(QFrame::Box);
        setLineWidth(2);
        
        mainLayout = new QGridLayout(this);
        mainLayout->setSpacing(2);
        mainLayout->setContentsMargins(5, 5, 5, 5);
        
        // Column widths: Name(25%), Frames(60%), Averages(7.5%), Total(7.5%)
        
        // BOWLER NAME (Left column)
        nameLabel = new QLabel(bowlerData.name, this);
        nameLabel->setFont(QFont("Arial", 18, QFont::Bold));
        nameLabel->setAlignment(Qt::AlignCenter);
        nameLabel->setStyleSheet("QLabel { border: 1px solid black; padding: 10px; }");
        mainLayout->addWidget(nameLabel, 0, 0, 2, 1);  // Spans 2 rows
        
        // FRAMES AREA (Center columns)
        createFrameDisplay();
        
        // AVERAGE/HANDICAP (Right-center column)
        if (displayOptions.contains("show_average") || displayOptions.contains("show_handicap")) {
            createAverageHandicapDisplay();
        }
        
        // TOTAL SCORE (Far right column)
        createTotalScoreDisplay();
        
        updateHighlight(isCurrentPlayer);
    }
    
    void createFrameDisplay() {
        // Determine frame display mode
        QString displayMode = displayOptions.value("frame_mode", "ten_frame").toString();
        int framesToShow = (displayMode == "four_frame") ? 4 : 10;
        int startFrame = displayOptions.value("frame_start", 0).toInt();
        
        frameWidgets.clear();
        
        for (int i = 0; i < framesToShow; ++i) {
            int frameIndex = startFrame + i;
            if (frameIndex >= 10) break;
            
            // Frame container
            QFrame* frameContainer = new QFrame(this);
            frameContainer->setFrameStyle(QFrame::Box);
            frameContainer->setMinimumSize(80, 60);
            
            QVBoxLayout* frameLayout = new QVBoxLayout(frameContainer);
            frameLayout->setSpacing(1);
            frameLayout->setContentsMargins(2, 2, 2, 2);
            
            // Frame header
            QLabel* frameHeader = new QLabel(QString("F%1").arg(frameIndex + 1), frameContainer);
            frameHeader->setAlignment(Qt::AlignCenter);
            frameHeader->setFont(QFont("Arial", 8, QFont::Bold));
            frameHeader->setMaximumHeight(15);
            
            // Ball results area
            QHBoxLayout* ballsLayout = new QHBoxLayout();
            ballsLayout->setSpacing(1);
            
            // Create ball result labels (up to 3)
            QVector<QLabel*> ballLabels;
            for (int j = 0; j < 3; ++j) {
                QLabel* ballLabel = new QLabel("-", frameContainer);
                ballLabel->setAlignment(Qt::AlignCenter);
                ballLabel->setFont(QFont("Arial", 10));
                ballLabel->setMinimumSize(20, 20);
                ballLabel->setStyleSheet("QLabel { border: 1px solid gray; }");
                ballLabels.append(ballLabel);
                ballsLayout->addWidget(ballLabel);
            }
            
            // Frame total
            QLabel* frameTotal = new QLabel("0", frameContainer);
            frameTotal->setAlignment(Qt::AlignCenter);
            frameTotal->setFont(QFont("Arial", 12, QFont::Bold));
            frameTotal->setMinimumHeight(25);
            frameTotal->setStyleSheet("QLabel { border: 1px solid black; background-color: lightgray; }");
            
            frameLayout->addWidget(frameHeader);
            frameLayout->addLayout(ballsLayout);
            frameLayout->addWidget(frameTotal);
            
            // Store widgets for updates
            FrameWidgetSet frameSet;
            frameSet.container = frameContainer;
            frameSet.ballLabels = ballLabels;
            frameSet.totalLabel = frameTotal;
            frameSet.frameIndex = frameIndex;
            frameWidgets.append(frameSet);
            
            // Add to main layout
            mainLayout->addWidget(frameContainer, 0, 1 + i, 2, 1);
        }
    }
    
    void createAverageHandicapDisplay() {
        QFrame* avgHdcpFrame = new QFrame(this);
        avgHdcpFrame->setFrameStyle(QFrame::Box);
        avgHdcpFrame->setMinimumSize(80, 60);
        
        QVBoxLayout* avgHdcpLayout = new QVBoxLayout(avgHdcpFrame);
        
        // Average
        if (displayOptions.contains("average")) {
            QLabel* avgLabel = new QLabel("AVG", avgHdcpFrame);
            avgLabel->setAlignment(Qt::AlignCenter);
            avgLabel->setFont(QFont("Arial", 8));
            
            avgValueLabel = new QLabel(QString::number(displayOptions.value("average", 0).toInt()), avgHdcpFrame);
            avgValueLabel->setAlignment(Qt::AlignCenter);
            avgValueLabel->setFont(QFont("Arial", 12, QFont::Bold));
            
            avgHdcpLayout->addWidget(avgLabel);
            avgHdcpLayout->addWidget(avgValueLabel);
        }
        
        // Handicap
        if (displayOptions.contains("handicap")) {
            QLabel* hdcpLabel = new QLabel("HDCP", avgHdcpFrame);
            hdcpLabel->setAlignment(Qt::AlignCenter);
            hdcpLabel->setFont(QFont("Arial", 8));
            
            hdcpValueLabel = new QLabel(QString::number(displayOptions.value("handicap", 0).toInt()), avgHdcpFrame);
            hdcpValueLabel->setAlignment(Qt::AlignCenter);
            hdcpValueLabel->setFont(QFont("Arial", 12, QFont::Bold));
            
            avgHdcpLayout->addWidget(hdcpLabel);
            avgHdcpLayout->addWidget(hdcpValueLabel);
        }
        
        int col = displayOptions.value("frame_mode", "ten_frame").toString() == "four_frame" ? 5 : 11;
        mainLayout->addWidget(avgHdcpFrame, 0, col, 2, 1);
    }
    
    void createTotalScoreDisplay() {
        QFrame* totalFrame = new QFrame(this);
        totalFrame->setFrameStyle(QFrame::Box);
        totalFrame->setMinimumSize(100, 60);
        
        QVBoxLayout* totalLayout = new QVBoxLayout(totalFrame);
        
        // Scratch score
        scratchScoreLabel = new QLabel(QString::number(bowlerData.totalScore), totalFrame);
        scratchScoreLabel->setAlignment(Qt::AlignCenter);
        scratchScoreLabel->setFont(QFont("Arial", 16, QFont::Bold));
        scratchScoreLabel->setStyleSheet("QLabel { color: black; }");
        
        totalLayout->addWidget(scratchScoreLabel);
        
        // With handicap score (if applicable)
        QString totalDisplayMode = displayOptions.value("total_display", "Scratch").toString();
        if (totalDisplayMode != "Scratch" && displayOptions.contains("handicap")) {
            int handicap = displayOptions.value("handicap", 0).toInt();
            int withHandicap = bowlerData.totalScore + handicap;
            
            withHandicapLabel = new QLabel(QString("(%1)").arg(withHandicap), totalFrame);
            withHandicapLabel->setAlignment(Qt::AlignCenter);
            withHandicapLabel->setFont(QFont("Arial", 12));
            withHandicapLabel->setStyleSheet("QLabel { color: blue; }");
            
            totalLayout->addWidget(withHandicapLabel);
        }
        
        // 3-6-9 status if applicable
        if (displayOptions.contains("three_six_nine_status")) {
            QString status = displayOptions.value("three_six_nine_status").toString();
            if (!status.isEmpty()) {
                threeSixNineLabel = new QLabel(status, totalFrame);
                threeSixNineLabel->setAlignment(Qt::AlignCenter);
                threeSixNineLabel->setFont(QFont("Arial", 10));
                threeSixNineLabel->setStyleSheet("QLabel { color: green; }");
                totalLayout->addWidget(threeSixNineLabel);
            }
        }
        
        int col = displayOptions.value("frame_mode", "ten_frame").toString() == "four_frame" ? 6 : 12;
        mainLayout->addWidget(totalFrame, 0, col, 2, 1);
    }
    
    void updateDisplay() {
        // Update frame displays
        for (const FrameWidgetSet& frameSet : frameWidgets) {
            updateFrameWidget(frameSet);
        }
        
        // Update totals
        scratchScoreLabel->setText(QString::number(bowlerData.totalScore));
        if (withHandicapLabel && displayOptions.contains("handicap")) {
            int handicap = displayOptions.value("handicap", 0).toInt();
            int withHandicap = bowlerData.totalScore + handicap;
            withHandicapLabel->setText(QString("(%1)").arg(withHandicap));
        }
    }
    
    void updateFrameWidget(const FrameWidgetSet& frameSet) {
        if (frameSet.frameIndex >= bowlerData.frames.size()) return;
        
        const Frame& frame = bowlerData.frames[frameSet.frameIndex];
        
        // Update ball results
        for (int i = 0; i < frameSet.ballLabels.size(); ++i) {
            if (i < frame.balls.size()) {
                const Ball& ball = frame.balls[i];
                QString ballText = formatBallResult(ball, i, frame);
                frameSet.ballLabels[i]->setText(ballText);
            } else {
                frameSet.ballLabels[i]->setText("-");
            }
        }
        
        // Update frame total
        if (frame.isComplete) {
            frameSet.totalLabel->setText(QString::number(frame.totalScore));
        } else {
            frameSet.totalLabel->setText("...");
        }
    }
    
    QString formatBallResult(const Ball& ball, int ballIndex, const Frame& frame) {
        if (ball.value == 15) {
            return "X"; // Strike
        } else if (ballIndex > 0) {
            // Check for spare
            int runningTotal = 0;
            for (int i = 0; i <= ballIndex; ++i) {
                runningTotal += frame.balls[i].value;
            }
            if (runningTotal == 15) {
                return "/"; // Spare
            }
        }
        return QString::number(ball.value);
    }
    
private:
    struct FrameWidgetSet {
        QFrame* container;
        QVector<QLabel*> ballLabels;
        QLabel* totalLabel;
        int frameIndex;
    };
    
    Bowler bowlerData;
    bool isCurrentPlayer;
    QJsonObject displayOptions;
    
    QGridLayout* mainLayout;
    QLabel* nameLabel;
    QLabel* scratchScoreLabel;
    QLabel* withHandicapLabel = nullptr;
    QLabel* avgValueLabel = nullptr;
    QLabel* hdcpValueLabel = nullptr;
    QLabel* threeSixNineLabel = nullptr;
    
    QVector<FrameWidgetSet> frameWidgets;
};

// BowlerWidget Game Status Widget
GameStatusWidget::GameStatusWidget(QWidget* parent) 
    : QFrame(parent), statusLabel(nullptr), frameLabel(nullptr), ballLabel(nullptr), pinDisplay(nullptr) {
    setupUI();
    setFrameStyle(QFrame::Box);
    setLineWidth(2);
}

void GameStatusWidget::setupUI() {
    mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(10, 5, 10, 5);
    mainLayout->setSpacing(15);
    
    // Status text (current player)
    statusLabel = new QLabel("Waiting for game...", this);
    statusLabel->setFont(QFont("Arial", 16, QFont::Bold));
    statusLabel->setAlignment(Qt::AlignCenter);
    statusLabel->setMinimumWidth(200);
    
    // Frame number
    frameLabel = new QLabel("Frame: -", this);
    frameLabel->setFont(QFont("Arial", 14, QFont::Bold));
    frameLabel->setAlignment(Qt::AlignCenter);
    frameLabel->setMinimumWidth(80);
    
    // Ball number  
    ballLabel = new QLabel("Ball: -", this);
    ballLabel->setFont(QFont("Arial", 14, QFont::Bold));
    ballLabel->setAlignment(Qt::AlignCenter);
    ballLabel->setMinimumWidth(70);
    
    // Pin display
    pinDisplay = new PinDisplayWidget(this);
    pinDisplay->setDisplayMode("mini");
    pinDisplay->setMinimumSize(100, 75);
    pinDisplay->setMaximumSize(120, 75);
    
    // Add to layout
    mainLayout->addWidget(statusLabel, 1);
    mainLayout->addWidget(frameLabel, 0);
    mainLayout->addWidget(ballLabel, 0);
    mainLayout->addWidget(pinDisplay, 0);
    mainLayout->addStretch();
    
    // Default styling
    setStyleSheet("QFrame { background-color: #3c3c3c; color: white; }", "white");
}

void GameStatusWidget::updateStatus(const QString& bowlerName, int frame, int ball, const QVector<int>& pinStates) {
    if (statusLabel) {
        statusLabel->setText(QString("Current Player: %1").arg(bowlerName));
    }
    
    if (frameLabel) {
        frameLabel->setText(QString("Frame: %1").arg(frame + 1)); // Convert to 1-based
    }
    
    if (ballLabel) {
        ballLabel->setText(QString("Ball: %1").arg(ball + 1)); // Convert to 1-based
    }
    
    if (pinDisplay && pinStates.size() == 5) {
        pinDisplay->setPinStates(pinStates);
    }
}

void GameStatusWidget::updateBallNumber(int ballNumber) {
    if (ballLabel) {
        ballLabel->setText(QString("Ball: %1").arg(ballNumber));
    }
}

void GameStatusWidget::updateFrameNumber(int frameNumber) {
    if (frameLabel) {
        frameLabel->setText(QString("Frame: %1").arg(frameNumber));
    }
}

void GameStatusWidget::resetStatus() {
    if (statusLabel) {
        statusLabel->setText("Waiting for game...");
    }
    
    if (frameLabel) {
        frameLabel->setText("Frame: -");
    }
    
    if (ballLabel) {
        ballLabel->setText("Ball: -");
    }
    
    if (pinDisplay) {
        pinDisplay->resetPins();
    }
}

void GameStatusWidget::setStyleSheet(const QString& background, const QString& foreground) {
    QString style = QString(R"(
        QFrame {
            background-color: %1;
            color: %2;
            border: 2px solid %2;
            border-radius: 5px;
        }
        QLabel {
            background-color: transparent;
            color: %2;
        }
    )").arg(background, foreground);
    
    QFrame::setStyleSheet(style);
}

// BowlerWidget implementation
BowlerWidget::BowlerWidget(const Bowler& bowler, bool isCurrentPlayer, QWidget* parent)
    : QFrame(parent), bowlerData(bowler), isCurrentPlayer(isCurrentPlayer), 
      compactMode(false), showDetails(true), scoreAnimation(nullptr), 
      playerChangeAnimation(nullptr), opacityEffect(nullptr) {
    
    setupUI(isCurrentPlayer);
    updateDisplay();
}

void BowlerWidget::updateBowler(const Bowler& bowler, bool isCurrentPlayer) {
    bowlerData = bowler;
    this->isCurrentPlayer = isCurrentPlayer;
    updateHighlight(isCurrentPlayer);
    updateDisplay();
}

void BowlerWidget::updateHighlight(bool isCurrentPlayer) {
    this->isCurrentPlayer = isCurrentPlayer;
    
    if (isCurrentPlayer) {
        setStyleSheet("QFrame { background-color: yellow; border: 3px solid red; }");
    } else {
        setStyleSheet("QFrame { background-color: lightblue; border: 1px solid black; }");
    }
}

void BowlerWidget::setColorScheme(const QString& background, const QString& foreground, 
                                 const QString& highlight, const QString& current) {
    backgroundColor = background;
    foregroundColor = foreground;
    highlightColor = highlight;
    currentPlayerColor = current;
    
    updateHighlight(isCurrentPlayer);
}

void BowlerWidget::animateScoreUpdate(int frameIndex) {
    if (frameIndex >= 0 && frameIndex < totalLabels.size()) {
        QLabel* label = totalLabels[frameIndex];
        
        if (!scoreAnimation) {
            scoreAnimation = new QPropertyAnimation(label, "styleSheet");
            connect(scoreAnimation, &QPropertyAnimation::finished, this, &BowlerWidget::onAnimationFinished);
        }
        
        scoreAnimation->setDuration(500);
        scoreAnimation->setStartValue("QLabel { background-color: white; }");
        scoreAnimation->setEndValue("QLabel { background-color: yellow; }");
        scoreAnimation->start();
    }
}

void BowlerWidget::animatePlayerChange(bool isBecomingCurrent) {
    if (!opacityEffect) {
        opacityEffect = new QGraphicsOpacityEffect(this);
        setGraphicsEffect(opacityEffect);
    }
    
    if (!playerChangeAnimation) {
        playerChangeAnimation = new QPropertyAnimation(opacityEffect, "opacity");
        connect(playerChangeAnimation, &QPropertyAnimation::finished, this, &BowlerWidget::onAnimationFinished);
    }
    
    playerChangeAnimation->setDuration(300);
    if (isBecomingCurrent) {
        playerChangeAnimation->setStartValue(0.7);
        playerChangeAnimation->setEndValue(1.0);
    } else {
        playerChangeAnimation->setStartValue(1.0);
        playerChangeAnimation->setEndValue(0.8);
    }
    playerChangeAnimation->start();
}

void BowlerWidget::setCompactMode(bool compact) {
    compactMode = compact;
    updateDisplay();
}

void BowlerWidget::setShowDetails(bool showDetails) {
    this->showDetails = showDetails;
    updateDisplay();
}

void BowlerWidget::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        emit bowlerClicked(bowlerData.name);
    }
    QFrame::mousePressEvent(event);
}

void BowlerWidget::paintEvent(QPaintEvent* event) {
    QFrame::paintEvent(event);
    
    // Additional custom painting if needed
}

void BowlerWidget::onAnimationFinished() {
    // Reset styles after animation
    if (sender() == scoreAnimation) {
        // Reset score label style
    } else if (sender() == playerChangeAnimation) {
        // Animation complete
    }
}

void BowlerWidget::setupUI(bool isCurrentPlayer) {
    setFrameStyle(QFrame::Box);
    updateHighlight(isCurrentPlayer);
    
    mainLayout = new QGridLayout(this);
    
    // Bowler name
    nameLabel = new QLabel(bowlerData.name, this);
    nameLabel->setFont(QFont("Arial", 20, QFont::Bold));
    nameLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(nameLabel, 0, 0, 1, 11);
    
    createFrameWidgets();
}

void BowlerWidget::createFrameWidgets() {
    // Frame headers
    QStringList headers = {"F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10", "Total"};
    for (int i = 0; i < headers.size(); ++i) {
        QLabel* header = new QLabel(headers[i], this);
        header->setFont(QFont("Arial", 12, QFont::Bold));
        header->setAlignment(Qt::AlignCenter);
        header->setStyleSheet("QLabel { border: 1px solid black; background-color: lightgray; }");
        mainLayout->addWidget(header, 1, i);
    }
    
    // Frame displays
    frameLabels.resize(10);
    totalLabels.resize(10);
    
    for (int i = 0; i < 10; ++i) {
        // Ball results
        frameLabels[i] = new QLabel(this);
        frameLabels[i]->setAlignment(Qt::AlignCenter);
        frameLabels[i]->setStyleSheet("QLabel { border: 1px solid black; background-color: white; }");
        frameLabels[i]->setMinimumHeight(compactMode ? 20 : 30);
        mainLayout->addWidget(frameLabels[i], 2, i);
        
        // Frame totals
        totalLabels[i] = new QLabel(this);
        totalLabels[i]->setAlignment(Qt::AlignCenter);
        totalLabels[i]->setStyleSheet("QLabel { border: 1px solid black; background-color: white; }");
        totalLabels[i]->setFont(QFont("Arial", compactMode ? 12 : 14, QFont::Bold));
        totalLabels[i]->setMinimumHeight(compactMode ? 30 : 40);
        mainLayout->addWidget(totalLabels[i], 3, i);
    }
    
    // Grand total
    grandTotalLabel = new QLabel(this);
    grandTotalLabel->setAlignment(Qt::AlignCenter);
    grandTotalLabel->setStyleSheet("QLabel { border: 2px solid black; background-color: yellow; }");
    grandTotalLabel->setFont(QFont("Arial", compactMode ? 18 : 24, QFont::Bold));
    grandTotalLabel->setMinimumHeight(compactMode ? 50 : 70);
    mainLayout->addWidget(grandTotalLabel, 2, 10, 2, 1);
}

void BowlerWidget::updateDisplay() {
    nameLabel->setText(bowlerData.name);
    
    for (int i = 0; i < 10; ++i) {
        updateFrameDisplay(i);
    }
    
    // Update grand total
    grandTotalLabel->setText(QString::number(bowlerData.totalScore));
}

void BowlerWidget::updateFrameDisplay(int frameIndex) {
    if (frameIndex < 0 || frameIndex >= bowlerData.frames.size()) return;
    
    const Frame& frame = bowlerData.frames[frameIndex];
    
    // Update ball display
    frameLabels[frameIndex]->setText(frame.getDisplayText());
    
    // Update frame total
    if (frame.isComplete) {
        totalLabels[frameIndex]->setText(QString::number(frame.totalScore));
    } else if (!frame.balls.isEmpty()) {
        totalLabels[frameIndex]->setText("...");
    } else {
        totalLabels[frameIndex]->setText("");
    }
}

// BowlerListWidget implementation
BowlerListWidget::BowlerListWidget(QWidget* parent) 
    : QScrollArea(parent), currentBowlerIndex(0), maxVisibleBowlers(6), 
      animationEnabled(true), compactMode(false), rotationAnimation(nullptr) {
    setupUI();
}

void BowlerListWidget::updateBowlers(const QVector<Bowler>& bowlers, int currentBowlerIndex) {
    this->bowlers = bowlers;
    this->currentBowlerIndex = currentBowlerIndex;
    
    // Set display order to prioritize current player
    displayOrder.clear();
    if (!bowlers.isEmpty() && currentBowlerIndex >= 0 && currentBowlerIndex < bowlers.size()) {
        displayOrder.append(currentBowlerIndex);
        
        // Add other players
        for (int i = 0; i < bowlers.size() && displayOrder.size() < maxVisibleBowlers; ++i) {
            if (i != currentBowlerIndex) {
                displayOrder.append(i);
            }
        }
    }
    
    rebuildBowlerList();
}

void BowlerListWidget::setBowlerOrder(const QVector<int>& order) {
    displayOrder = order;
    rebuildBowlerList();
}

void BowlerListWidget::setMaxVisibleBowlers(int maxVisible) {
    maxVisibleBowlers = maxVisible;
    rebuildBowlerList();
}

void BowlerListWidget::setAnimationEnabled(bool enabled) {
    animationEnabled = enabled;
}

void BowlerListWidget::animatePlayerRotation(int fromIndex, int toIndex) {
    Q_UNUSED(fromIndex)
    Q_UNUSED(toIndex)
    
    if (!animationEnabled) {
        emit rotationAnimationFinished();
        return;
    }
    
    // Simple fade animation
    if (!rotationAnimation) {
        rotationAnimation = new QPropertyAnimation(this, "opacity");
        connect(rotationAnimation, &QPropertyAnimation::finished, this, &BowlerListWidget::onRotationAnimationFinished);
    }
    
    rotationAnimation->setDuration(300);
    rotationAnimation->setStartValue(1.0);
    rotationAnimation->setEndValue(0.8);
    rotationAnimation->start();
}

void BowlerListWidget::setColorScheme(const QJsonObject& colors) {
    colorScheme = colors;
    rebuildBowlerList();
}

void BowlerListWidget::setCompactMode(bool compact) {
    compactMode = compact;
    rebuildBowlerList();
}

void BowlerListWidget::onBowlerClicked(const QString& bowlerName) {
    for (int i = 0; i < bowlers.size(); ++i) {
        if (bowlers[i].name == bowlerName) {
            emit bowlerSelected(i);
            break;
        }
    }
}

void BowlerListWidget::onRotationAnimationFinished() {
    emit rotationAnimationFinished();
}

void BowlerListWidget::setupUI() {
    contentWidget = new QWidget(this);
    contentLayout = new QVBoxLayout(contentWidget);
    contentLayout->setSpacing(5);
    contentLayout->setContentsMargins(5, 5, 5, 5);
    
    setWidget(contentWidget);
    setWidgetResizable(true);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
}

void BowlerListWidget::rebuildBowlerList() {
    // Clear existing widgets
    for (BowlerWidget* widget : bowlerWidgets) {
        widget->deleteLater();
    }
    bowlerWidgets.clear();
    
    // Create new bowler widgets
    for (int i = 0; i < displayOrder.size() && i < maxVisibleBowlers; ++i) {
        int bowlerIndex = displayOrder[i];
        if (bowlerIndex >= 0 && bowlerIndex < bowlers.size()) {
            bool isCurrent = (bowlerIndex == currentBowlerIndex);
            
            BowlerWidget* bowlerWidget = new BowlerWidget(bowlers[bowlerIndex], isCurrent, this);
            bowlerWidget->setCompactMode(compactMode);
            
            connect(bowlerWidget, &BowlerWidget::bowlerClicked, this, &BowlerListWidget::onBowlerClicked);
            
            contentLayout->addWidget(bowlerWidget);
            bowlerWidgets.append(bowlerWidget);
        }
    }
    
    contentLayout->addStretch();
    updateBowlerHighlights();
}

void BowlerListWidget::updateBowlerHighlights() {
    for (int i = 0; i < bowlerWidgets.size(); ++i) {
        if (i < displayOrder.size()) {
            int bowlerIndex = displayOrder[i];
            bool isCurrent = (bowlerIndex == currentBowlerIndex);
            bowlerWidgets[i]->updateHighlight(isCurrent);
        }
    }
}

QWidget* BowlerListWidget::createBowlerWidget(const Bowler& bowler, bool isCurrentPlayer) {
    return new BowlerWidget(bowler, isCurrentPlayer, this);
}

// GameControlWidget implementation
GameControlWidget::GameControlWidget(QWidget* parent) 
    : QFrame(parent), gameType("quick_game"), isHeld(false) {
    setupUI();
    setFrameStyle(QFrame::Box);
}

void GameControlWidget::setButtonsEnabled(bool hold, bool skip, bool reset) {
    holdButton->setEnabled(hold);
    skipButton->setEnabled(skip);
    resetButton->setEnabled(reset);
}

void GameControlWidget::setHoldButtonState(bool isHeld) {
    this->isHeld = isHeld;
    updateButtonStates();
}

void GameControlWidget::setGameType(const QString& gameType) {
    this->gameType = gameType;
    
    // Show/hide buttons based on game type
    if (gameType == "quick_game") {
        holdButton->show();
        skipButton->show();
        resetButton->show();
        settingsButton->hide();
    } else if (gameType == "league_game") {
        holdButton->show();
        skipButton->hide();
        resetButton->show();
        settingsButton->show();
    }
}

void GameControlWidget::setButtonColors(const QJsonObject& colors) {
    buttonColors = colors;
    updateButtonStates();
}

void GameControlWidget::setButtonSize(const QSize& size) {
    holdButton->setMinimumSize(size);
    skipButton->setMinimumSize(size);
    resetButton->setMinimumSize(size);
    settingsButton->setMinimumSize(size);
}

void GameControlWidget::setupUI() {
    buttonLayout = new QHBoxLayout(this);
    
    holdButton = new QPushButton("HOLD", this);
    skipButton = new QPushButton("SKIP", this);
    resetButton = new QPushButton("RESET", this);
    settingsButton = new QPushButton("SETTINGS", this);
    
    // Set default button properties
    QFont buttonFont("Arial", 18, QFont::Bold);
    QSize buttonSize(120, 60);
    
    holdButton->setFont(buttonFont);
    holdButton->setMinimumSize(buttonSize);
    
    skipButton->setFont(buttonFont);
    skipButton->setMinimumSize(buttonSize);
    
    resetButton->setFont(buttonFont);
    resetButton->setMinimumSize(buttonSize);
    
    settingsButton->setFont(buttonFont);
    settingsButton->setMinimumSize(buttonSize);
    
    // Connect signals
    connect(holdButton, &QPushButton::clicked, this, &GameControlWidget::holdClicked);
    connect(skipButton, &QPushButton::clicked, this, &GameControlWidget::skipClicked);
    connect(resetButton, &QPushButton::clicked, this, &GameControlWidget::resetClicked);
    connect(settingsButton, &QPushButton::clicked, this, &GameControlWidget::settingsClicked);
    
    // Add buttons to layout
    buttonLayout->addWidget(holdButton);
    buttonLayout->addWidget(skipButton);
    buttonLayout->addWidget(resetButton);
    buttonLayout->addWidget(settingsButton);
    buttonLayout->addStretch();
    
    updateButtonStates();
}

void GameControlWidget::updateButtonStates() {
    // Update hold button
    if (isHeld) {
        holdButton->setText("RESUME");
        holdButton->setStyleSheet("QPushButton { background-color: red; color: white; font-size: 18px; font-weight: bold; }");
    } else {
        holdButton->setText("HOLD");
        holdButton->setStyleSheet("QPushButton { background-color: green; color: white; font-size: 18px; font-weight: bold; }");
    }
    
    // Apply custom colors if available
    if (!buttonColors.isEmpty()) {
        QString holdColor = isHeld ? 
            buttonColors["hold_active"].toString("red") : 
            buttonColors["hold_inactive"].toString("green");
        
        holdButton->setStyleSheet(QString(
            "QPushButton { background-color: %1; color: white; font-size: 18px; font-weight: bold; }"
        ).arg(holdColor));
        
        QString skipColor = buttonColors["skip"].toString("orange");
        skipButton->setStyleSheet(QString(
            "QPushButton { background-color: %1; color: white; font-size: 18px; font-weight: bold; }"
        ).arg(skipColor));
        
        QString resetColor = buttonColors["reset"].toString("darkred");
        resetButton->setStyleSheet(QString(
            "QPushButton { background-color: %1; color: white; font-size: 18px; font-weight: bold; }"
        ).arg(resetColor));
    }
}

// ScrollTextWidget implementation
ScrollTextWidget::ScrollTextWidget(QWidget* parent) 
    : QLabel(parent), scrollSpeed(50), scrollDirection("left"), 
      scrollPosition(0), textWidth(0), isScrolling(false) {
    
    scrollTimer = new QTimer(this);
    connect(scrollTimer, &QTimer::timeout, this, &ScrollTextWidget::onScrollTimer);
    
    scrollFont = QFont("Arial", 16);
    setFont(scrollFont);
    setAlignment(Qt::AlignVCenter);
    setStyleSheet("QLabel { background-color: black; color: yellow; }");
}

void ScrollTextWidget::setText(const QString& text) {
    scrollText = text;
    calculateScrollParameters();
    scrollPosition = 0;
    update();
}

void ScrollTextWidget::setScrollSpeed(int pixelsPerSecond) {
    scrollSpeed = pixelsPerSecond;
    
    if (isScrolling) {
        int interval = 1000 / scrollSpeed; // Update interval in ms
        scrollTimer->setInterval(qMax(10, interval)); // Minimum 10ms
    }
}

void ScrollTextWidget::setScrollDirection(const QString& direction) {
    scrollDirection = direction;
    scrollPosition = 0;
    update();
}

void ScrollTextWidget::startScrolling() {
    if (!isScrolling && !scrollText.isEmpty()) {
        isScrolling = true;
        int interval = 1000 / scrollSpeed;
        scrollTimer->start(qMax(10, interval));
    }
}

void ScrollTextWidget::stopScrolling() {
    isScrolling = false;
    scrollTimer->stop();
}

void ScrollTextWidget::pauseScrolling() {
    scrollTimer->stop();
}

void ScrollTextWidget::paintEvent(QPaintEvent* event) {
    QPainter painter(this);
    painter.setFont(scrollFont);
    painter.setPen(QColor("yellow"));
    
    QRect textRect = rect();
    
    if (isScrolling && textWidth > width()) {
        // Draw scrolling text
        int x = scrollDirection == "left" ? 
            width() - scrollPosition : 
            scrollPosition - textWidth;
        
        painter.drawText(x, textRect.center().y() + fontMetrics().height()/4, scrollText);
        
        // Draw wrapped text if needed
        if (scrollDirection == "left" && x + textWidth < width()) {
            painter.drawText(x + textWidth + 50, textRect.center().y() + fontMetrics().height()/4, scrollText);
        } else if (scrollDirection == "right" && x > 0) {
            painter.drawText(x - textWidth - 50, textRect.center().y() + fontMetrics().height()/4, scrollText);
        }
    } else {
        // Draw static text
        painter.drawText(textRect, Qt::AlignCenter, scrollText);
    }
}

void ScrollTextWidget::resizeEvent(QResizeEvent* event) {
    QLabel::resizeEvent(event);
    calculateScrollParameters();
}

void ScrollTextWidget::onScrollTimer() {
    scrollPosition += 1;
    
    // Reset position when text has scrolled completely
    if (scrollDirection == "left") {
        if (scrollPosition > textWidth + width() + 50) {
            scrollPosition = 0;
        }
    } else {
        if (scrollPosition > textWidth + width() + 50) {
            scrollPosition = 0;
        }
    }
    
    update();
}

void ScrollTextWidget::calculateScrollParameters() {
    QFontMetrics fm(scrollFont);
    textWidth = fm.horizontalAdvance(scrollText);
}