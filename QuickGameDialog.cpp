#include "QuickGameDialog.h"
#include <QMessageBox>
#include <QGroupBox>

QuickGameDialog::QuickGameDialog(QWidget *parent)
    : QDialog(parent)
    , m_mainLayout(nullptr)
    , m_laneEdit(nullptr)
    , m_gamesSpinBox(nullptr)
    , m_timeSpinBox(nullptr)
    , m_framesSpinBox(nullptr)
    , m_totalDisplayCombo(nullptr)
{
    setupUI();
    setWindowTitle("Quick Game Setup");
    setMinimumSize(600, 700);
}

void QuickGameDialog::setupUI()
{
    m_mainLayout = new QVBoxLayout(this);
    
    // Lane selection
    QGroupBox *laneGroup = new QGroupBox("Lane Selection");
    QHBoxLayout *laneLayout = new QHBoxLayout(laneGroup);
    laneLayout->addWidget(new QLabel("Lane:"));
    m_laneEdit = new QLineEdit("1");
    m_laneEdit->setMaximumWidth(100);
    laneLayout->addWidget(m_laneEdit);
    laneLayout->addStretch();
    m_mainLayout->addWidget(laneGroup);
    
    // Bowlers section
    QGroupBox *bowlersGroup = new QGroupBox("Bowlers");
    QGridLayout *bowlersLayout = new QGridLayout(bowlersGroup);
    
    // Headers
    bowlersLayout->addWidget(new QLabel("Bowler"), 0, 0);
    bowlersLayout->addWidget(new QLabel("Name"), 0, 1);
    bowlersLayout->addWidget(new QLabel("Shoes"), 0, 2);
    bowlersLayout->addWidget(new QLabel("Youth"), 0, 3);
    bowlersLayout->addWidget(new QLabel("Pre-bowl"), 0, 4);
    
    // Create bowler rows
    for (int i = 0; i < MAX_BOWLERS; ++i) {
        int row = i + 1;
        
        // Bowler number
        bowlersLayout->addWidget(new QLabel(QString::number(i + 1)), row, 0);
        
        // Name edit
        QLineEdit *nameEdit = new QLineEdit;
        nameEdit->setPlaceholderText("Enter bowler name");
        m_bowlerEdits.append(nameEdit);
        bowlersLayout->addWidget(nameEdit, row, 1);
        
        // Shoes checkbox
        QCheckBox *shoesCheck = new QCheckBox;
        m_shoesCheckBoxes.append(shoesCheck);
        bowlersLayout->addWidget(shoesCheck, row, 2);
        
        // Youth checkbox
        QCheckBox *youthCheck = new QCheckBox;
        m_youthCheckBoxes.append(youthCheck);
        bowlersLayout->addWidget(youthCheck, row, 3);
        
        // Pre-bowl checkbox
        QCheckBox *prebowlCheck = new QCheckBox;
        m_prebowlCheckBoxes.append(prebowlCheck);
        bowlersLayout->addWidget(prebowlCheck, row, 4);
    }
    
    m_mainLayout->addWidget(bowlersGroup);
    
    // Game settings
    QGroupBox *settingsGroup = new QGroupBox("Game Settings");
    QGridLayout *settingsLayout = new QGridLayout(settingsGroup);
    
    // Number of games
    settingsLayout->addWidget(new QLabel("Number of Games:"), 0, 0);
    m_gamesSpinBox = new QSpinBox;
    m_gamesSpinBox->setMinimum(0);
    m_gamesSpinBox->setMaximum(10);
    m_gamesSpinBox->setValue(0);
    settingsLayout->addWidget(m_gamesSpinBox, 0, 1);
    
    // Time
    settingsLayout->addWidget(new QLabel("Time (30min blocks):"), 1, 0);
    m_timeSpinBox = new QSpinBox;
    m_timeSpinBox->setMinimum(0);
    m_timeSpinBox->setMaximum(10);
    m_timeSpinBox->setValue(0);
    settingsLayout->addWidget(m_timeSpinBox, 1, 1);
    
    // Frames per turn
    settingsLayout->addWidget(new QLabel("Frames per Turn:"), 2, 0);
    m_framesSpinBox = new QSpinBox;
    m_framesSpinBox->setMinimum(1);
    m_framesSpinBox->setMaximum(10);
    m_framesSpinBox->setValue(1);
    settingsLayout->addWidget(m_framesSpinBox, 2, 1);
    
    // Total display mode
    settingsLayout->addWidget(new QLabel("Scoring Display:"), 3, 0);
    m_totalDisplayCombo = new QComboBox;
    m_totalDisplayCombo->addItems({"Regular", "Handicap", "Regular Mix", 
                                  "POA (Percentage of Average)", "All Displays"});
    settingsLayout->addWidget(m_totalDisplayCombo, 3, 1);
    
    m_mainLayout->addWidget(settingsGroup);
    
    // Buttons
    QHBoxLayout *buttonLayout = new QHBoxLayout;
    
    QPushButton *clearButton = new QPushButton("Clear");
    connect(clearButton, &QPushButton::clicked, this, &QuickGameDialog::onClearClicked);
    buttonLayout->addWidget(clearButton);
    
    buttonLayout->addStretch();
    
    QPushButton *cancelButton = new QPushButton("Cancel");
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    buttonLayout->addWidget(cancelButton);
    
    QPushButton *startButton = new QPushButton("Start Game");
    connect(startButton, &QPushButton::clicked, this, &QuickGameDialog::onAccepted);
    buttonLayout->addWidget(startButton);
    
    m_mainLayout->addLayout(buttonLayout);
}

void QuickGameDialog::onClearClicked()
{
    clearForm();
}

void QuickGameDialog::clearForm()
{
    m_laneEdit->setText("1");
    
    for (QLineEdit *edit : m_bowlerEdits) {
        edit->clear();
    }
    
    for (QCheckBox *check : m_shoesCheckBoxes) {
        check->setChecked(false);
    }
    
    for (QCheckBox *check : m_youthCheckBoxes) {
        check->setChecked(false);
    }
    
    for (QCheckBox *check : m_prebowlCheckBoxes) {
        check->setChecked(false);
    }
    
    m_gamesSpinBox->setValue(0);
    m_timeSpinBox->setValue(0);
    m_framesSpinBox->setValue(1);
    m_totalDisplayCombo->setCurrentIndex(0);
}

void QuickGameDialog::onAccepted()
{
    // Validate input
    bool ok;
    int laneId = m_laneEdit->text().toInt(&ok);
    if (!ok || laneId <= 0) {
        QMessageBox::warning(this, "Invalid Input", "Please enter a valid lane number.");
        return;
    }
    
    // Get bowler names
    QStringList bowlerNames;
    for (QLineEdit *edit : m_bowlerEdits) {
        QString name = edit->text().trimmed();
        if (!name.isEmpty()) {
            bowlerNames.append(name);
        }
    }
    
    if (bowlerNames.isEmpty()) {
        QMessageBox::warning(this, "No Bowlers", "Please enter at least one bowler name.");
        return;
    }
    
    // Validate games vs time
    int games = m_gamesSpinBox->value();
    int time = m_timeSpinBox->value();
    
    if (games > 0 && time > 0) {
        QMessageBox::warning(this, "Invalid Input", 
                           "Only one of 'Number of Games' or 'Time' can be set.");
        return;
    }
    
    if (games == 0 && time == 0) {
        QMessageBox::warning(this, "Invalid Input", 
                           "Either 'Number of Games' or 'Time' must be set.");
        return;
    }
    
    QJsonObject gameData = getGameData();
    emit gameStartRequested(gameData);
    
    accept();
}

QJsonObject QuickGameDialog::getGameData() const
{
    QJsonObject data;
    
    // Basic info
    data["lane_id"] = m_laneEdit->text().toInt();
    data["type"] = "quick_game";
    
    // Bowlers
    QJsonArray bowlers;
    QJsonArray preBowlBowlers;
    
    for (int i = 0; i < m_bowlerEdits.size(); ++i) {
        QString name = m_bowlerEdits[i]->text().trimmed();
        if (!name.isEmpty()) {
            QJsonObject bowler;
            bowler["name"] = name;
            bowler["shoes"] = m_shoesCheckBoxes[i]->isChecked();
            bowler["youth"] = m_youthCheckBoxes[i]->isChecked();
            
            if (m_prebowlCheckBoxes[i]->isChecked()) {
                preBowlBowlers.append(name);
            }
            
            bowlers.append(bowler);
        }
    }
    
    data["bowlers"] = bowlers;
    data["pre_bowl"] = preBowlBowlers;
    
    // Game settings
    int games = m_gamesSpinBox->value();
    int time = m_timeSpinBox->value();
    
    if (games > 0) {
        data["games"] = games;
        data["time"] = QJsonValue(); // null
    } else {
        data["games"] = QJsonValue(); // null
        data["time"] = time * 30; // Convert to minutes
    }
    
    data["frames_per_turn"] = m_framesSpinBox->value();
    data["total_display"] = m_totalDisplayCombo->currentText();
    
    return data;
}