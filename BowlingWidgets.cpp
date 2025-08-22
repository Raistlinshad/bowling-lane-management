#include "BowlingWidgets.h"
#include <QMouseEvent>
#include <QKeyEvent>
#include <QResizeEvent>
#include <QPaintEvent>
#include <QDebug>
#include <QEasingCurve>

// Static constants for PinDisplayWidget
const QVector<QPointF> PinDisplayWidget::pinPositions = {
    QPointF(0.2, 0.6),   // lTwo (left)
    QPointF(0.5, 0.2),   // lThree (top)
    QPointF(0.5, 0.5),   // cFive (center)
    QPointF(0.5, 0.8),   // rThree (bottom)
    QPointF(0.8, 0.6)    // rTwo (right)
};

const QStringList PinDisplayWidget::pinNames = {"L2", "L3", "C5", "R3", "R2"};
const QVector<int> PinDisplayWidget::pinValues = {2, 3, 5, 3, 2};

// PinDisplayWidget implementation
PinDisplayWidget::PinDisplayWidget(QWidget* parent) 
    : QWidget(parent), displayMode("large"), upColor("white"), downColor("black"), 
      pinAnimation(nullptr), isAnimating(false) {
    
    pinStates.resize(5);
    resetPins();
    setupPinLayout();
    setMinimumSize(200, 150);
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
    pinAnimation->setDuration(1000);
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
        setMinimumSize(120, 90);
    } else if (mode == "mini") {
        setMinimumSize(80, 60);
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
    
    for (int i = 0; i < 5; ++i) {
        QRect pinRect = getPinRect(i);
        bool isUp = pinStates[i] == 1;
        
        if (isAnimating) {
            // During animation, interpolate between start and end states
            // For simplicity, just use the final state
            isUp = animationEndStates[i] == 1;
        }
        
        drawPin(painter, i, pinRect, isUp);
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

void PinDisplayWidget::drawPin(QPainter& painter, int pinIndex, const QRect& rect, bool isUp) {
    // Set pin color
    QColor color = isUp ? QColor(upColor) : QColor(downColor);
    painter.setBrush(QBrush(color));
    painter.setPen(QPen(Qt::black, 2));
    
    // Draw pin as an ellipse
    painter.drawEllipse(rect);
    
    // Draw pin label
    painter.setPen(Qt::black);
    QFont font = painter.font();
    font.setPointSize(displayMode == "mini" ? 8 : (displayMode == "small" ? 10 : 12));
    font.setBold(true);
    painter.setFont(font);
    
    painter.drawText(rect, Qt::AlignCenter, pinNames[pinIndex]);
    
    // Draw pin value
    if (displayMode != "mini") {
        QRect valueRect = rect.adjusted(0, rect.height() * 0.7, 0, 0);
        font.setPointSize(font.pointSize() - 2);
        painter.setFont(font);
        painter.drawText(valueRect, Qt::AlignCenter, QString::number(pinValues[pinIndex]));
    }
}

QRect PinDisplayWidget::getPinRect(int pinIndex) const {
    if (pinIndex < 0 || pinIndex >= pinPositions.size()) {
        return QRect();
    }
    
    QPointF pos = pinPositions[pinIndex];
    QSize widgetSize = size();
    
    int pinSize = displayMode == "mini" ? 20 : (displayMode == "small" ? 25 : 35);
    
    int x = static_cast<int>(pos.x() * widgetSize.width() - pinSize / 2);
    int y = static_cast<int>(pos.y() * widgetSize.height() - pinSize / 2);
    
    return QRect(x, y, pinSize, pinSize);
}

// GameStatusWidget implementation
GameStatusWidget::GameStatusWidget(QWidget* parent) : QFrame(parent) {
    setupUI();
    setFrameStyle(QFrame::Box);
    QFrame::setStyleSheet("QFrame { background-color: darkblue; color: white; }");
}

void GameStatusWidget::updateStatus(const QString& bowlerName, int frame, int ball, const QVector<int>& pinStates) {
    statusLabel->setText(QString("Current Player: %1").arg(bowlerName));
    frameLabel->setText(QString("Frame: %1").arg(frame + 1));
    ballLabel->setText(QString("Ball: %1").arg(ball + 1));
    pinDisplay->setPinStates(pinStates);
}

void GameStatusWidget::updateBallNumber(int ballNumber) {
    ballLabel->setText(QString("Ball: %1").arg(ballNumber));
}

void GameStatusWidget::updateFrameNumber(int frameNumber) {
    frameLabel->setText(QString("Frame: %1").arg(frameNumber));
}

void GameStatusWidget::resetStatus() {
    statusLabel->setText("Waiting for game...");
    frameLabel->setText("Frame: -");
    ballLabel->setText("Ball: -");
    pinDisplay->resetPins();
}

void GameStatusWidget::setStyleSheet(const QString& background, const QString& foreground) {
    QFrame::setStyleSheet(QString("QFrame { background-color: %1; color: %2; }").arg(background, foreground));
}

void GameStatusWidget::setupUI() {
    mainLayout = new QHBoxLayout(this);
    
    // Status information
    QVBoxLayout* infoLayout = new QVBoxLayout();
    
    statusLabel = new QLabel("Waiting for game...", this);
    statusLabel->setFont(QFont("Arial", 16, QFont::Bold));
    
    frameLabel = new QLabel("Frame: -", this);
    frameLabel->setFont(QFont("Arial", 14));
    
    ballLabel = new QLabel("Ball: -", this);
    ballLabel->setFont(QFont("Arial", 14));
    
    infoLayout->addWidget(statusLabel);
    infoLayout->addWidget(frameLabel);
    infoLayout->addWidget(ballLabel);
    
    // Pin display
    pinDisplay = new PinDisplayWidget(this);
    pinDisplay->setDisplayMode("small");
    
    mainLayout->addLayout(infoLayout, 1);
    mainLayout->addWidget(pinDisplay, 0);
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
