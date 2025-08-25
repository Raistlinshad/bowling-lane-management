// GameRecoveryManager.h

class GameRecoveryManager : public QObject {
    Q_OBJECT
    
public:
    explicit GameRecoveryManager(QObject* parent = nullptr);
    
    // Recovery file management
    void markGameActive(int gameNumber, const QJsonObject& gameState);
    void markGameInactive();
    bool hasActiveGame() const;
    QJsonObject getActiveGameData() const;
    
    // Boot recovery dialog
    void checkForRecovery(QWidget* parent);
    
signals:
    void recoveryRequested(const QJsonObject& gameState);
    void recoveryDeclined();
    
private slots:
    void onRecoveryTimeout();
    
private:
    void saveRecoveryState();
    void loadRecoveryState();
    void showRecoveryDialog(QWidget* parent);
    
    QString recoveryFilePath;
    QJsonObject currentRecoveryData;
    bool gameActive;
    int gameNumber;
    QTimer* recoveryTimer;
};

