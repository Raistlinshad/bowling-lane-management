#include <QApplication>
#include <QMainWindow>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QFrame>
#include <QScrollArea>
#include <QTimer>
#include <QStackedWidget>
#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>
#include <QMediaPlayer>
#include <QVideoWidget>
#include <QPixmap>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QSettings>
#include <QFile>
#include <QDir>
#include <QRandomGenerator>
#include <QDebug>
#include <QProcess>
#include "LaneClient.h"
#include "QuickGame.h"
#include "BowlingWidgets.h"
#include "MediaManager.h"

// Forward declarations
class BowlingMainWindow;


// Bowler display widget
class BowlerWidget : public QFrame {
    Q_OBJECT
    
public:
    BowlerWidget(const Bowler& bowler, bool isCurrentPlayer = false, QWidget* parent = nullptr) 
        : QFrame(parent), bowlerData(bowler) {
        setupUI(isCurrentPlayer);
        updateDisplay();
    }
    
    void updateBowler(const Bowler& bowler, bool isCurrentPlayer = false) {
        bowlerData = bowler;
        updateHighlight(isCurrentPlayer);
        updateDisplay();
    }
    
    void updateHighlight(bool isCurrentPlayer) {
        if (isCurrentPlayer) {
            setStyleSheet("QFrame { background-color: yellow; border: 3px solid red; }");
        } else {
            setStyleSheet("QFrame { background-color: lightblue; border: 1px solid black; }");
        }
    }

private:
    void setupUI(bool isCurrentPlayer) {
        setFrameStyle(QFrame::Box);
        updateHighlight(isCurrentPlayer);
        
        QGridLayout* layout = new QGridLayout(this);
        
        // Bowler name
        nameLabel = new QLabel(bowlerData.name, this);
        nameLabel->setFont(QFont("Arial", 20, QFont::Bold));
        nameLabel->setAlignment(Qt::AlignCenter);
        layout->addWidget(nameLabel, 0, 0, 1, 11);
        
        // Frame headers
        QStringList headers = {"F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10", "Total"};
        for (int i = 0; i < headers.size(); ++i) {
            QLabel* header = new QLabel(headers[i], this);
            header->setFont(QFont("Arial", 12, QFont::Bold));
            header->setAlignment(Qt::AlignCenter);
            header->setStyleSheet("QLabel { border: 1px solid black; background-color: lightgray; }");
            layout->addWidget(header, 1, i);
        }
        
        // Frame displays
        frameLabels.resize(10);
        totalLabels.resize(10);
        
        for (int i = 0; i < 10; ++i) {
            // Ball results
            frameLabels[i] = new QLabel(this);
            frameLabels[i]->setAlignment(Qt::AlignCenter);
            frameLabels[i]->setStyleSheet("QLabel { border: 1px solid black; background-color: white; }");
            frameLabels[i]->setMinimumHeight(30);
            layout->addWidget(frameLabels[i], 2, i);
            
            // Frame totals
            totalLabels[i] = new QLabel(this);
            totalLabels[i]->setAlignment(Qt::AlignCenter);
            totalLabels[i]->setStyleSheet("QLabel { border: 1px solid black; background-color: white; }");
            totalLabels[i]->setFont(QFont("Arial", 14, QFont::Bold));
            totalLabels[i]->setMinimumHeight(40);
            layout->addWidget(totalLabels[i], 3, i);
        }
        
        // Total score
        grandTotalLabel = new QLabel(this);
        grandTotalLabel->setAlignment(Qt::AlignCenter);
        grandTotalLabel->setStyleSheet("QLabel { border: 2px solid black; background-color: yellow; }");
        grandTotalLabel->setFont(QFont("Arial", 24, QFont::Bold));
        grandTotalLabel->setMinimumHeight(70);
        layout->addWidget(grandTotalLabel, 2, 10, 2, 1);
    }
    
    void updateDisplay() {
        nameLabel->setText(bowlerData.name);
        
        for (int i = 0; i < 10; ++i) {
            const Frame& frame = bowlerData.frames[i];
            
            // Update ball display
            frameLabels[i]->setText(frame.getDisplayText());
            
            // Update frame total
            if (frame.isComplete) {
                totalLabels[i]->setText(QString::number(frame.totalScore));
            } else if (!frame.balls.isEmpty()) {
                totalLabels[i]->setText("...");
            } else {
                totalLabels[i]->setText("");
            }
        }
        
        // Update grand total
        grandTotalLabel->setText(QString::number(bowlerData.totalScore));
    }
    
    Bowler bowlerData;
    QLabel* nameLabel;
    QVector<QLabel*> frameLabels;
    QVector<QLabel*> totalLabels;
    QLabel* grandTotalLabel;
};

// Media display widget for ads and special effects
class MediaDisplayWidget : public QStackedWidget {
    Q_OBJECT
    
public:
    MediaDisplayWidget(QWidget* parent = nullptr) : QStackedWidget(parent) {
        setupUI();
        setupMediaRotation();
    }
    
    void playSpecialEffect(const QString& effectName, int duration = 3000) {
        qDebug() << "Playing special effect:" << effectName;
        
        if (effectName == "strike") {
            showStrikeEffect();
        } else if (effectName == "spare") {
            showSpareEffect();
        }
        
        // Return to game display after effect
        QTimer::singleShot(duration, this, [this]() {
            setCurrentIndex(0); // Return to game display
        });
    }
    
    void setGameWidget(QWidget* gameWidget) {
        if (indexOf(gameWidget) == -1) {
            insertWidget(0, gameWidget);
        }
        setCurrentIndex(0);
    }

private slots:
    void rotateMedia() {
        if (currentIndex() == 0) return; // Don't rotate if showing game
        
        // Implement media rotation logic here
        // For now, just cycle through available widgets
    }
    
    void showStrikeEffect() {
        QLabel* strikeLabel = new QLabel("STRIKE!", this);
        strikeLabel->setAlignment(Qt::AlignCenter);
        strikeLabel->setStyleSheet("QLabel { background-color: red; color: white; font-size: 72px; font-weight: bold; }");
        
        addWidget(strikeLabel);
        setCurrentWidget(strikeLabel);
        
        // Remove the label after use
        QTimer::singleShot(3000, this, [this, strikeLabel]() {
            removeWidget(strikeLabel);
            strikeLabel->deleteLater();
        });
    }
    
    void showSpareEffect() {
        QLabel* spareLabel = new QLabel("SPARE!", this);
        spareLabel->setAlignment(Qt::AlignCenter);
        spareLabel->setStyleSheet("QLabel { background-color: blue; color: white; font-size: 72px; font-weight: bold; }");
        
        addWidget(spareLabel);
        setCurrentWidget(spareLabel);
        
        // Remove the label after use
        QTimer::singleShot(3000, this, [this, spareLabel]() {
            removeWidget(spareLabel);
            spareLabel->deleteLater();
        });
    }

private:
    void setupUI() {
        // Default placeholder widget
        QLabel* placeholder = new QLabel("Ready for Game", this);
        placeholder->setAlignment(Qt::AlignCenter);
        placeholder->setStyleSheet("QLabel { background-color: blue; color: white; font-size: 48px; }");
        addWidget(placeholder);
    }
    
    void setupMediaRotation() {
        mediaTimer = new QTimer(this);
        connect(mediaTimer, &QTimer::timeout, this, &MediaDisplayWidget::rotateMedia);
        mediaTimer->start(300000); // 5 minutes
    }
    
    QTimer* mediaTimer;
};

private:
    MediaDisplayWidget* mediaDisplay;
    QScrollArea* gameDisplayArea;
    QWidget* gameWidget;
    QVBoxLayout* gameLayout;
    GameStatusWidget* gameStatus;
    
    QPushButton* holdButton;
    QPushButton* skipButton;
    QPushButton* resetButton;
    QLabel* scrollLabel = nullptr;
    
    LaneClient* client;
    QuickGame* game;
};

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    
    // Set application properties
    app.setApplicationName("Canadian5PinBowling");
    app.setApplicationVersion("1.0");
    app.setOrganizationName("BowlingCenter");
    
    BowlingMainWindow window;
    window.show();
    
    return app.exec();
}

#include "main.moc"