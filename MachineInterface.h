#ifndef MACHINE_INTERFACE_H
#define MACHINE_INTERFACE_H

#include <QObject>
#include <QTimer>
#include <QJsonObject>
#include <QVector>
#include <QDebug>

// Raspberry Pi GPIO access
#ifdef __arm__
#include <wiringPi.h>
#include <wiringPiI2C.h>
#define GPIO_AVAILABLE
#endif

class MachineInterface : public QObject {
    Q_OBJECT
    
public:
    explicit MachineInterface(QObject* parent = nullptr);
    ~MachineInterface();
    
    // Initialization
    bool initialize();
    void shutdown();
    
    // Ball detection control
    void startBallDetection();
    void stopBallDetection();
    void setDetectionSuspended(bool suspended);
    
    // Machine control
    void resetPins(bool immediate = false);
    void setPinConfiguration(const QVector<int>& pinStates);
    void setGameActive(bool active);
    
    // State queries
    QVector<int> getCurrentPinStates() const { return currentPinStates; }
    bool isDetectionActive() const { return detectionActive; }
    bool isDetectionSuspended() const { return detectionSuspended; }

public slots:
    void onBallDetectionTimer();
    void onMachineTimer();

signals:
    // Main signal - emits [#,#,#,#,#] format
    void ballDetected(const QVector<int>& pinStates);
    
    void machineReady();
    void machineError(const QString& error);
    void pinStatesChanged(const QVector<int>& states);

private:
    // Hardware setup
    bool setupGPIO();
    bool setupADS();
    void loadSettings();
    
    // Ball detection
    void checkBallSensor();
    QVector<int> readPinSensors();
    
    // Machine operations
    void executePinReset();
    void executePinConfiguration(const QVector<int>& states);
    bool waitForB21Sensor(int timeoutMs = 8000);

    enum MachineState {
        IDLE,           // Ready for ball detection
        RESETTING,      // Currently resetting pins
        SETTING_PINS,   // Currently setting specific pin configuration
        WAITING_B21     // Waiting for machine timing sensor
    };
    
    MachineState currentState;
    bool gameActive;
    
    // GPIO pin assignments (from settings.json)
    int gp1, gp2, gp3, gp4, gp5, gp6, gp7, gp8;
    
    // ADS I2C handles for pin sensors
    #ifdef GPIO_AVAILABLE
        // ADS1115 sensor reading methods
        float readADS1115Channel(int adsHandle, int channel, int timeoutMs);
        int getPinIndexFromName(const QString& pinName);
    #endif
    
    // Timers
    QTimer* ballDetectionTimer;
    QTimer* machineTimer;
    
    // Detection state
    bool detectionActive;
    bool detectionSuspended;
    int ballDetectionCounter;
    int detectionThreshold;
    qint64 lastDetectionTime;
    int debounceTimeMs;
    
    // Pin states - Canadian 5-pin format: [lTwo, lThree, cFive, rThree, rTwo]
    QVector<int> currentPinStates;  // Current detected states (1=up, 0=down)
    QVector<int> targetPinStates;   // Target states for machine to set
    
    // Machine operation state
    bool machineInOperation;
    qint64 resetStartTime;
    double machineCycleTimeSeconds;
    
    // Configuration
    int laneId;
    QJsonObject laneSettings;
    QString pb10, pb11, pb12, pb13, pb20; // Pin sensor mappings

    // Add ADS1115 register definitions to MachineInterface.h
    #ifdef GPIO_AVAILABLE
    // ADS1115 register addresses
    #define ADS1115_REG_CONVERSION  0x00
    #define ADS1115_REG_CONFIG      0x01
    #define ADS1115_REG_LO_THRESH   0x02
    #define ADS1115_REG_HI_THRESH   0x03

    // ADS1115 configuration values
    #define ADS1115_CONFIG_OS_SINGLE    (1 << 15)  // Start single conversion
    #define ADS1115_CONFIG_MUX_AIN0     (0x04 << 12)  // AIN0
    #define ADS1115_CONFIG_MUX_AIN1     (0x05 << 12)  // AIN1  
    #define ADS1115_CONFIG_MUX_AIN2     (0x06 << 12)  // AIN2
    #define ADS1115_CONFIG_MUX_AIN3     (0x07 << 12)  // AIN3
    #define ADS1115_CONFIG_PGA_6_144V   (0x00 << 9)   // +/-6.144V range
    #define ADS1115_CONFIG_MODE_SINGLE  (1 << 8)      // Single-shot mode
    #define ADS1115_CONFIG_DR_128SPS    (0x00 << 5)   // 128 samples per second
    #define ADS1115_CONFIG_CMODE_TRAD   (0 << 4)      // Traditional comparator
    #define ADS1115_CONFIG_CPOL_ACTVLOW (0 << 3)      // Alert/Ready pin low
    #define ADS1115_CONFIG_CLAT_NONLAT  (0 << 2)      // Non-latching comparator
    #define ADS1115_CONFIG_CQUE_NONE    (3 << 0)      // Disable comparator
    #endif

};

#endif // MACHINE_INTERFACE_H
