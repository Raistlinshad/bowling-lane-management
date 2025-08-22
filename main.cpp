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

// Forward declarations
class BowlingMainWindow;

// Pin display widget
class PinDisplayWidget : public QWidget {
    Q_OBJECT
    
public:
    PinDisplayWidget(QWidget* parent = nullptr) : QWidget(parent) {
        setupUI();
        resetPins();
    }
    
    void setPinStates(const QVector<int>& states) {
        pinStates = states;
        updatePinDisplay();
    }
    
    void resetPins() {
        pinStates = QVector<int>(5, 1); // All pins up
        updatePinDisplay();
    }

private slots:
    void updatePinDisplay() {
        // Pin layout: lTwo, lThree, cFive, rThree, rTwo
        const QStringList pinNames = {"L2", "L3", "C5", "R3", "R2"};
        
        for (int i = 0; i < 5; ++i) {
            QLabel* pinLabel = pinLabels[i];
            if (pinStates[i] == 0) {
                // Pin down - black
                pinLabel->setStyleSheet("QLabel { background-color: black; color: white; border: 2px solid white; }");
            } else {
                // Pin up - white
                pinLabel->setStyleSheet("QLabel { background-color: white; color: black; border: 2px solid black; }");
            }
            pinLabel->setText(pinNames[i]);
        }
    }
    
private:
    void setupUI() {
        QGridLayout* layout = new QGridLayout(this);
        layout->setSpacing(5);
        
        // Canadian 5-pin layout
        //     L3
        //  L2    R2
        //     C5
        //     R3
        
        pinLabels.resize(5);
        
        // Create pin labels
        for (int i = 0; i < 5; ++i) {
            pinLabels[i] = new QLabel(this);
            pinLabels[i]->setAlignment(Qt::AlignCenter);
            pinLabels[i]->setMinimumSize(40, 60);
            pinLabels[i]->setMaximumSize(40, 60);
            pinLabels[i]->setFont(QFont("Arial", 10, QFont::Bold));
        }
        
        // Position pins (lTwo=0, lThree=1, cFive=2, rThree=3, rTwo=4)
        layout->addWidget(pinLabels[1], 0, 1);  // L3 top center
        layout->addWidget(pinLabels[0], 1, 0);  // L2 left
        layout->addWidget(pinLabels[4], 1, 2);  // R2 right
        layout->addWidget(pinLabels[2], 2, 1);  // C5 center
        layout->addWidget(pinLabels[3], 3, 1);  // R3 bottom center
    }
    
    QVector<QLabel*> pinLabels;
    QVector<int> pinStates;
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


// Machine interface helper
class MachineInterface : public QObject {
    Q_OBJECT
    
public:
    MachineInterface(QObject* parent = nullptr) : QObject(parent) {
        pythonProcess = new QProcess(this);
        connect(pythonProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, &MachineInterface::onProcessFinished);
        connect(pythonProcess, &QProcess::readyReadStandardOutput,
                this, &MachineInterface::onDataReady);
    }
    
    void startDetection() {
        if (pythonProcess->state() == QProcess::NotRunning) {
            pythonProcess->start("python3", QStringList() << "machine_interface.py");
        }
    }
    
    void stopDetection() {
        if (pythonProcess->state() != QProcess::NotRunning) {
            pythonProcess->terminate();
            if (!pythonProcess->waitForFinished(3000)) {
                pythonProcess->kill();
            }
        }
    }

signals:
    void ballDetected(const QVector<int>& pins);
    void machineError(const QString& error);

private slots:
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus) {
        Q_UNUSED(exitStatus)
        if (exitCode != 0) {
            emit machineError("Machine process crashed");
        }
    }
    
    void onDataReady() {
        QByteArray data = pythonProcess->readAllStandardOutput();
        QStringList lines = QString::fromUtf8(data).split('\n', Qt::SkipEmptyParts);
        
        for (const QString& line : lines) {
            QJsonDocument doc = QJsonDocument::fromJson(line.toUtf8());
            if (doc.isObject()) {
                QJsonObject obj = doc.object();
                if (obj["type"].toString() == "ball_detected") {
                    QJsonArray pinsArray = obj["pins"].toArray();
                    QVector<int> pins;
                    for (const QJsonValue& value : pinsArray) {
                        pins.append(value.toInt());
                    }
                    if (pins.size() == 5) {
                        emit ballDetected(pins);
                    }
                }
            }
        }
    }

private:
    QProcess* pythonProcess;
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