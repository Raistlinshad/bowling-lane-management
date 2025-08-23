#ifndef QUICKGAMEDIALOG_H
#define QUICKGAMEDIALOG_H

#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QJsonObject>
#include <QJsonArray>

class QuickGameDialog : public QDialog
{
    Q_OBJECT

public:
    explicit QuickGameDialog(QWidget *parent = nullptr);
    
    QJsonObject getGameData() const;

private slots:
    void onClearClicked();
    void onAccepted();

private:
    void setupUI();
    void clearForm();
    
    QVBoxLayout *m_mainLayout;
    QLineEdit *m_laneEdit;
    QList<QLineEdit*> m_bowlerEdits;
    QList<QCheckBox*> m_shoesCheckBoxes;
    QList<QCheckBox*> m_youthCheckBoxes;
    QList<QCheckBox*> m_prebowlCheckBoxes;
    QSpinBox *m_gamesSpinBox;
    QSpinBox *m_timeSpinBox;
    QSpinBox *m_framesSpinBox;
    QComboBox *m_totalDisplayCombo;
    
    static const int MAX_BOWLERS = 10;

signals:
    void gameStartRequested(const QJsonObject &gameData);

};

#endif // QUICKGAMEDIALOG_H