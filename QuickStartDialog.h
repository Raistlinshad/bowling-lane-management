#ifndef QUICKSTARTDIALOG_H
#define QUICKSTARTDIALOG_H

#include <QDialog>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QFrame>

class MainWindow;

class QuickStartDialog : public QDialog
{
    Q_OBJECT

public:
    explicit QuickStartDialog(MainWindow *mainWindow, QWidget *parent = nullptr);

private slots:
    void onQuickGameClicked();
    void onBingoClicked();
    void onPracticeClicked();
    void onPartyClicked();
    void onHorseClicked();
    void onGolfClicked();
    void onBaseballClicked();
    void onSettingsClicked();
    void onSetupClicked();
    void onEndDayClicked();
    void onNewGameTypeClicked();

private:
    void setupUI();
    QPushButton* createImageButton(const QString &text, const QString &iconPath = "");
    
    MainWindow *m_mainWindow;
    QVBoxLayout *m_mainLayout;
    QGridLayout *m_buttonGrid1;
    QGridLayout *m_buttonGrid2;
    QGridLayout *m_buttonGrid3;
};

#endif // QUICKSTARTDIALOG_H