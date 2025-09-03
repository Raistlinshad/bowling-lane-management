#ifndef BOWLINGWIDGETS_H
#define BOWLINGWIDGETS_H

// Qt5 compatible includes
#include <QWidget>
#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QVector>
#include <QTimer>
#include <QPainter>
#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QJsonObject>
#include <QString>
#include <QPointF>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QResizeEvent>
#include <QPaintEvent>
#include <QDebug>
#include <QEasingCurve>
#include <QFont>
#include <QFontMetrics>
#include <QColor>
#include <QBrush>
#include <QPen>
#include <QRect>
#include <QSize>
#include "QuickGame.h"

// Forward declarations
class QuickGame;
class Bowler;
class Frame;
class Ball;

// Canadian 5-pin display widget
class PinDisplayWidget : public QWidget {
    Q_OBJECT
    Q_PROPERTY(qreal animationProgress READ animationProgress WRITE setAnimationProgress)
    
public:
    explicit PinDisplayWidget(QWidget* parent = nullptr);
    
    void setPinStates(const QVector<int>& states);
    void resetPins();
    void animatePinFall(const QVector<int>& beforeStates, const QVector<int>& afterStates);
    
    void setDisplayMode(const QString& mode); // "large", "small", "mini"
    void setColorScheme(const QString& upColor, const QString& downColor);
    
    // Animation property
    qreal animationProgress() const { return m_animationProgress; }
    void setAnimationProgress(qreal progress) { m_animationProgress = progress; update(); }

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void onAnimationFinished();

private:
    void setupPinLayout();
    void updatePinDisplay();
    void drawPin(QPainter& painter, int pinIndex, const QRect& rect, bool isUp, qreal opacity = 1.0);
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
    qreal m_animationProgress;
    
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
    
    void updateStatus(const QString& bowlerName, int frame, int ball);
    void updateBallNumber(int ballNumber);
    void updateFrameNumber(int frameNumber);
    void resetStatus();
    
    void setStyleSheet(const QString& background, const QString& foreground);
    void setGameStyleSheet(const QString& background, const QString& foreground); // For main.cpp compatibility

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

// Enhanced BowlerWidget for advanced frame layout
class EnhancedBowlerWidget : public QFrame {
    Q_OBJECT
    
public:
    explicit EnhancedBowlerWidget(const Bowler& bowler, bool isCurrentPlayer = false, 
                                 const QJsonObject& displayOptions = QJsonObject(), 
                                 QWidget* parent = nullptr);
    
    void updateBowler(const Bowler& bowler, bool isCurrentPlayer = false);
    void updateHighlight(bool isCurrentPlayer);
    void setDisplayOptions(const QJsonObject& options);

signals:
    void bowlerClicked(const QString& bowlerName);

protected:
    void mousePressEvent(QMouseEvent* event) override;

private:
    struct FrameWidgetSet {
        QFrame* container;
        QVector<QLabel*> ballLabels;
        QLabel* totalLabel;
        int frameIndex;
    };

    void setupEnhancedUI();
    void createFrameDisplay();
    void createAverageHandicapDisplay(); 
    void createTotalScoreDisplay();
    void updateDisplay();
    void updateFrameWidget(const FrameWidgetSet& frameSet);
    QString formatBallResult(const Ball& ball, int ballIndex, const Frame& frame);
    
    Bowler bowlerData;
    bool isCurrentPlayer;
    QJsonObject displayOptions;
    
    QGridLayout* mainLayout;
    QLabel* nameLabel;
    QLabel* scratchScoreLabel;
    QLabel* withHandicapLabel;
    QLabel* avgValueLabel;
    QLabel* hdcpValueLabel;
    QLabel* threeSixNineLabel;
    
    QVector<FrameWidgetSet> frameWidgets;
};


#endif // BOWLINGWIDGETS_H
