#ifndef BOWLINGWIDGETS_H
#define BOWLINGWIDGETS_H

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
#include "QuickGame.h"

// Forward declarations
class QuickGame;
class Bowler;
class Frame;

// Canadian 5-pin display widget
class PinDisplayWidget : public QWidget {
    Q_OBJECT
    
public:
    explicit PinDisplayWidget(QWidget* parent = nullptr);
    
    void setPinStates(const QVector<int>& states);
    void resetPins();
    void animatePinFall(const QVector<int>& beforeStates, const QVector<int>& afterStates);
    
    void setDisplayMode(const QString& mode); // "large", "small", "mini"
    void setColorScheme(const QString& upColor, const QString& downColor);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void onAnimationFinished();

private:
    void setupPinLayout();
    void updatePinDisplay();
    void drawPin(QPainter& painter, int pinIndex, const QRect& rect, bool isUp);
    QRect getPinRect(int pinIndex) const;
    
    QVector<int> pinStates;     // 0 = down, 1 = up
    QVector<QLabel*> pinLabels;
    
    QString displayMode;
    QString upColor;
    QString downColor;
    
    // Animation
    QPropertyAnimation* pinAnimation;
    QVector<int> animationStartStates;
    QVector<int> animationEndStates;
    bool isAnimating;
    
    // Layout positions for Canadian 5-pin
    static const QVector<QPointF> pinPositions;
    static const QStringList pinNames;
    static const QVector<int> pinValues;
};

// Game status display widget
class GameStatusWidget : public QFrame {
    Q_OBJECT
    
public:
    explicit GameStatusWidget(QWidget* parent = nullptr);
    
    void updateStatus(const QString& bowlerName, int frame, int ball, const QVector<int>& pinStates);
    void updateBallNumber(int ballNumber);
    void updateFrameNumber(int frameNumber);
    void resetStatus();
    
    void setStyleSheet(const QString& background, const QString& foreground);

private:
    void setupUI();
    
    QLabel* statusLabel;
    QLabel* frameLabel;
    QLabel* ballLabel;
    PinDisplayWidget* pinDisplay;
    QHBoxLayout* mainLayout;
};

// Individual bowler display widget
class BowlerWidget : public QFrame {
    Q_OBJECT
    
public:
    explicit BowlerWidget(const Bowler& bowler, bool isCurrentPlayer = false, QWidget* parent = nullptr);
    
    void updateBowler(const Bowler& bowler, bool isCurrentPlayer = false);
    void updateHighlight(bool isCurrentPlayer);
    void setColorScheme(const QString& background, const QString& foreground, 
                       const QString& highlight, const QString& current);
    
    // Animation support
    void animateScoreUpdate(int frameIndex);
    void animatePlayerChange(bool isBecomingCurrent);
    
    // Display modes
    void setCompactMode(bool compact);
    void setShowDetails(bool showDetails);

signals:
    void bowlerClicked(const QString& bowlerName);
    void frameClicked(const QString& bowlerName, int frameIndex);

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private slots:
    void onAnimationFinished();

private:
    void setupUI(bool isCurrentPlayer);
    void updateDisplay();
    void updateFrameDisplay(int frameIndex);
    void createFrameWidgets();
    
    Bowler bowlerData;
    
    // UI elements
    QLabel* nameLabel;
    QVector<QLabel*> frameLabels;      // Ball results
    QVector<QLabel*> totalLabels;     // Frame totals
    QLabel* grandTotalLabel;
    QGridLayout* mainLayout;
    
    // Display settings
    bool isCurrentPlayer;
    bool compactMode;
    bool showDetails;
    
    // Color scheme
    QString backgroundColor;
    QString foregroundColor;
    QString highlightColor;
    QString currentPlayerColor;
    
    // Animation
    QPropertyAnimation* scoreAnimation;
    QPropertyAnimation* playerChangeAnimation;
    QGraphicsOpacityEffect* opacityEffect;
};

// Scrollable bowler list widget
class BowlerListWidget : public QScrollArea {
    Q_OBJECT
    
public:
    explicit BowlerListWidget(QWidget* parent = nullptr);
    
    void updateBowlers(const QVector<Bowler>& bowlers, int currentBowlerIndex);
    void setBowlerOrder(const QVector<int>& order);
    void setMaxVisibleBowlers(int maxVisible);
    void setAnimationEnabled(bool enabled);
    
    // Player rotation animation
    void animatePlayerRotation(int fromIndex, int toIndex);
    
    // Display settings
    void setColorScheme(const QJsonObject& colors);
    void setCompactMode(bool compact);

signals:
    void bowlerSelected(int bowlerIndex);
    void rotationAnimationFinished();

private slots:
    void onBowlerClicked(const QString& bowlerName);
    void onRotationAnimationFinished();

private:
    void setupUI();
    void rebuildBowlerList();
    void updateBowlerHighlights();
    QWidget* createBowlerWidget(const Bowler& bowler, bool isCurrentPlayer);
    
    QWidget* contentWidget;
    QVBoxLayout* contentLayout;
    QVector<BowlerWidget*> bowlerWidgets;
    
    QVector<Bowler> bowlers;
    QVector<int> displayOrder;
    int currentBowlerIndex;
    int maxVisibleBowlers;
    bool animationEnabled;
    bool compactMode;
    
    // Animation
    QPropertyAnimation* rotationAnimation;
    
    // Color scheme
    QJsonObject colorScheme;
};

// Game control button widget
class GameControlWidget : public QFrame {
    Q_OBJECT
    
public:
    explicit GameControlWidget(QWidget* parent = nullptr);
    
    void setButtonsEnabled(bool hold, bool skip, bool reset);
    void setHoldButtonState(bool isHeld);
    void setGameType(const QString& gameType); // Affects which buttons are shown
    
    void setButtonColors(const QJsonObject& colors);
    void setButtonSize(const QSize& size);

signals:
    void holdClicked();
    void skipClicked();
    void resetClicked();
    void settingsClicked();

private:
    void setupUI();
    void updateButtonStates();
    
    QPushButton* holdButton;
    QPushButton* skipButton;
    QPushButton* resetButton;
    QPushButton* settingsButton;
    
    QHBoxLayout* buttonLayout;
    QString gameType;
    bool isHeld;
    QJsonObject buttonColors;
};

// Scroll text widget for announcements
class ScrollTextWidget : public QLabel {
    Q_OBJECT
    
public:
    explicit ScrollTextWidget(QWidget* parent = nullptr);
    
    void setText(const QString& text);
    void setScrollSpeed(int pixelsPerSecond);
    void setScrollDirection(const QString& direction); // "left", "right"
    
    void startScrolling();
    void stopScrolling();
    void pauseScrolling();

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void onScrollTimer();

private:
    void calculateScrollParameters();
    
    QString scrollText;
    int scrollSpeed;
    QString scrollDirection;
    int scrollPosition;
    int textWidth;
    bool isScrolling;
    
    QTimer* scrollTimer;
    QFont scrollFont;
};

// Main bowling display widget that combines all components
class BowlingDisplayWidget : public QWidget {
    Q_OBJECT
    
public:
    explicit BowlingDisplayWidget(QWidget* parent = nullptr);
    
    void setGame(QuickGame* game);
    void updateDisplay();
    void updateCurrentPlayer(int bowlerIndex);
    
    // Display configuration
    void setMaxVisibleBowlers(int maxVisible);
    void setColorScheme(const QJsonObject& colors);
    void setDisplayMode(const QString& mode); // "full", "compact", "minimal"
    
    // Animation control
    void setAnimationsEnabled(bool enabled);
    void animatePlayerRotation();
    void animateScoreUpdate(int bowlerIndex, int frameIndex);
    
    // Special display modes
    void showGameComplete(const QVector<Bowler>& finalStandings);
    void showFrameComplete(const Bowler& bowler, int frameIndex);
    
    // Server-controlled updates
    void handleDisplayUpdate(const QJsonObject& update);

signals:
    void bowlerSelected(int bowlerIndex);
    void gameControlActivated(const QString& action);

private slots:
    void onGameUpdated();
    void onCurrentPlayerChanged(int bowlerIndex);
    void onBowlerSelected(int bowlerIndex);
    void onGameControlClicked(const QString& action);
    void onScrollTextChanged(const QString& text);

private:
    void setupUI();
    void connectSignals();
    void updateGameStatus();
    void updateBowlerList();
    void updateControls();
    
    // UI Components
    GameStatusWidget* gameStatus;
    BowlerListWidget* bowlerList;
    GameControlWidget* gameControls;
    ScrollTextWidget* scrollText;
    
    QVBoxLayout* mainLayout;
    QHBoxLayout* bottomLayout;
    
    // Game reference
    QuickGame* game;
    
    // Display settings
    int maxVisibleBowlers;
    QJsonObject colorScheme;
    QString displayMode;
    bool animationsEnabled;
    
    // State tracking
    int lastCurrentBowler;
    QVector<int> lastScores;
};

// Frame detail popup widget (for detailed frame information)
class FrameDetailWidget : public QFrame {
    Q_OBJECT
    
public:
    explicit FrameDetailWidget(QWidget* parent = nullptr);
    
    void showFrameDetail(const Bowler& bowler, int frameIndex);
    void hideDetail();
    
    void setAutoHideDelay(int milliseconds);

signals:
    void detailClosed();

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private slots:
    void onAutoHideTimer();

private:
    void setupUI();
    void updateFrameDisplay(const Frame& frame, int frameIndex, bool isTenthFrame);
    void showAnimated();
    void hideAnimated();
    
    QLabel* titleLabel;
    QLabel* ballsLabel;
    QLabel* scoreLabel;
    QLabel* bonusLabel;
    PinDisplayWidget* pinDisplay;
    
    QVBoxLayout* mainLayout;
    QPropertyAnimation* showAnimation;
    QPropertyAnimation* hideAnimation;
    QTimer* autoHideTimer;
    
    int autoHideDelay;
    bool isVisible;
};

// Score animation widget for special effects
class ScoreAnimationWidget : public QWidget {
    Q_OBJECT
    
public:
    explicit ScoreAnimationWidget(QWidget* parent = nullptr);
    
    void animateStrike(const QPoint& position);
    void animateSpare(const QPoint& position);
    void animateScore(int score, const QPoint& position);
    void animateBonus(int bonus, const QPoint& position);
    
    void setAnimationStyle(const QString& style); // "popup", "fly", "fade"

signals:
    void animationFinished(const QString& type);

protected:
    void paintEvent(QPaintEvent* event) override;

private slots:
    void onAnimationFinished();
    void updateAnimation();

private:
    void setupAnimation(const QString& text, const QColor& color, const QPoint& position);
    void createPopupAnimation(const QPoint& position);
    void createFlyAnimation(const QPoint& position);
    void createFadeAnimation(const QPoint& position);
    
    QString animationText;
    QColor animationColor;
    QPoint animationPosition;
    QString animationStyle;
    QString currentAnimationType;
    
    QPropertyAnimation* positionAnimation;
    QPropertyAnimation* opacityAnimation;
    QPropertyAnimation* scaleAnimation;
    QTimer* updateTimer;
    
    qreal animationProgress;
    bool isAnimating;
};

// Tournament/League display variant (future expansion)
class TournamentDisplayWidget : public BowlingDisplayWidget {
    Q_OBJECT
    
public:
    explicit TournamentDisplayWidget(QWidget* parent = nullptr);
    
    void setTournamentMode(const QString& mode); // "elimination", "round_robin", "swiss"
    void updateTournamentStandings(const QJsonArray& standings);
    void showMatchup(const QJsonObject& matchup);

private:
    void setupTournamentUI();
    
    QString tournamentMode;
    QLabel* tournamentStatus;
    QWidget* standingsWidget;
};

#endif // BOWLINGWIDGETS_H