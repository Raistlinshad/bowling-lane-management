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
};

#endif // MACHINE_INTERFACE_H

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

// Updated readPinSensors method in MachineInterface.cpp
QVector<int> MachineInterface::readPinSensors() {
    QVector<int> pinStates = {1, 1, 1, 1, 1}; // Default: all pins up
    
#ifdef GPIO_AVAILABLE
    const float VOLTAGE_THRESHOLD = 4.0f; // 4V threshold for pin down detection
    const int MAX_RETRY_ATTEMPTS = 5;     // Maximum retry attempts per sensor
    const int RETRY_DELAY_MS = 10;        // Delay between retries
    const int CONVERSION_TIMEOUT_MS = 100; // Timeout for each conversion
    
    // Pin sensor mappings based on settings.json
    struct PinSensor {
        QString name;
        int adsHandle;
        int channel;
        int pinIndex;  // Index in pinStates array
    };
    
    // Map sensors to pins based on your settings
    QVector<PinSensor> sensors = {
        {pb10, ads1_handle, 0, getPinIndexFromName(pb10)},  // ADS1, Channel 0
        {pb11, ads1_handle, 1, getPinIndexFromName(pb11)},  // ADS1, Channel 1  
        {pb12, ads1_handle, 2, getPinIndexFromName(pb12)},  // ADS1, Channel 2
        {pb13, ads1_handle, 3, getPinIndexFromName(pb13)},  // ADS1, Channel 3
        {pb20, ads2_handle, 0, getPinIndexFromName(pb20)}   // ADS2, Channel 0
    };
    
    qint64 startTime = QDateTime::currentMSecsSinceEpoch();
    qint64 maxReadTime = 3000; // 3 second maximum for all sensors
    
    for (const PinSensor& sensor : sensors) {
        if (sensor.pinIndex < 0 || sensor.pinIndex >= 5) {
            qWarning() << "Invalid pin index for sensor" << sensor.name;
            continue;
        }
        
        bool sensorReadSuccessfully = false;
        int attempts = 0;
        
        while (!sensorReadSuccessfully && 
               attempts < MAX_RETRY_ATTEMPTS && 
               (QDateTime::currentMSecsSinceEpoch() - startTime) < maxReadTime) {
            
            attempts++;
            
            try {
                float voltage = readADS1115Channel(sensor.adsHandle, sensor.channel, CONVERSION_TIMEOUT_MS);
                
                if (voltage >= 0.0f) { // Valid reading
                    if (voltage >= VOLTAGE_THRESHOLD) {
                        pinStates[sensor.pinIndex] = 0; // Pin down
                        qDebug() << "Sensor" << sensor.name << "voltage:" << voltage << "V (PIN DOWN)";
                    } else {
                        pinStates[sensor.pinIndex] = 1; // Pin up  
                        qDebug() << "Sensor" << sensor.name << "voltage:" << voltage << "V (PIN UP)";
                    }
                    sensorReadSuccessfully = true;
                } else {
                    qWarning() << "Invalid reading from sensor" << sensor.name << "attempt" << attempts;
                    if (attempts < MAX_RETRY_ATTEMPTS) {
                        delay(RETRY_DELAY_MS); // Brief delay before retry
                    }
                }
                
            } catch (const std::exception& e) {
                qWarning() << "Exception reading sensor" << sensor.name << "attempt" << attempts << ":" << e.what();
                if (attempts < MAX_RETRY_ATTEMPTS) {
                    delay(RETRY_DELAY_MS);
                }
            } catch (...) {
                qWarning() << "Unknown error reading sensor" << sensor.name << "attempt" << attempts;
                if (attempts < MAX_RETRY_ATTEMPTS) {
                    delay(RETRY_DELAY_MS);
                }
            }
        }
        
        if (!sensorReadSuccessfully) {
            qError() << "FAILED to read sensor" << sensor.name << "after" << attempts << "attempts, using default PIN UP";
            pinStates[sensor.pinIndex] = 1; // Default to pin up on failure
        }
    }
    
    qDebug() << "Final pin states:" << pinStates;
    
#endif
    
    return pinStates;
}

#ifdef GPIO_AVAILABLE
// Helper method to read from specific ADS1115 channel with timeout
float MachineInterface::readADS1115Channel(int adsHandle, int channel, int timeoutMs) {
    if (adsHandle < 0) {
        throw std::runtime_error("Invalid ADS handle");
    }
    
    // Configure ADS1115 for single-shot conversion on specified channel
    uint16_t config = ADS1115_CONFIG_OS_SINGLE |      // Start conversion
                     ADS1115_CONFIG_PGA_6_144V |       // +/-6.144V range  
                     ADS1115_CONFIG_MODE_SINGLE |      // Single-shot mode
                     ADS1115_CONFIG_DR_128SPS |        // 128 SPS
                     ADS1115_CONFIG_CMODE_TRAD |       // Traditional comparator
                     ADS1115_CONFIG_CPOL_ACTVLOW |     // Active low
                     ADS1115_CONFIG_CLAT_NONLAT |      // Non-latching
                     ADS1115_CONFIG_CQUE_NONE;         // Disable comparator
    
    // Set channel
    switch (channel) {
        case 0: config |= ADS1115_CONFIG_MUX_AIN0; break;
        case 1: config |= ADS1115_CONFIG_MUX_AIN1; break;
        case 2: config |= ADS1115_CONFIG_MUX_AIN2; break;
        case 3: config |= ADS1115_CONFIG_MUX_AIN3; break;
        default: 
            throw std::runtime_error("Invalid ADS1115 channel: " + std::to_string(channel));
    }
    
    // Write configuration to start conversion
    if (wiringPiI2CWriteReg16(adsHandle, ADS1115_REG_CONFIG, config) < 0) {
        throw std::runtime_error("Failed to write ADS1115 config");
    }
    
    // Wait for conversion to complete
    qint64 startTime = QDateTime::currentMSecsSinceEpoch();
    
    while ((QDateTime::currentMSecsSinceEpoch() - startTime) < timeoutMs) {
        // Check if conversion is complete (OS bit = 1)
        int configRead = wiringPiI2CReadReg16(adsHandle, ADS1115_REG_CONFIG);
        if (configRead < 0) {
            throw std::runtime_error("Failed to read ADS1115 config status");
        }
        
        if (configRead & ADS1115_CONFIG_OS_SINGLE) {
            // Conversion complete, read result
            int raw = wiringPiI2CReadReg16(adsHandle, ADS1115_REG_CONVERSION);
            if (raw < 0) {
                throw std::runtime_error("Failed to read ADS1115 conversion result");
            }
            
            // Convert raw reading to voltage
            // ADS1115 returns 16-bit signed value, +/-6.144V range
            float voltage = ((int16_t)raw / 32768.0f) * 6.144f;
            
            return voltage;
        }
        
        delay(1); // Small delay before checking again
    }
    
    throw std::runtime_error("ADS1115 conversion timeout");
}

// Helper method to map pin names to array indices
int MachineInterface::getPinIndexFromName(const QString& pinName) {
    // Map pin sensor names to pin positions in [lTwo, lThree, cFive, rThree, rTwo]
    if (pinName == "lTwo") return 0;
    if (pinName == "lThree") return 1; 
    if (pinName == "cFive") return 2;
    if (pinName == "rThree") return 3;
    if (pinName == "rTwo") return 4;
    
    qWarning() << "Unknown pin name:" << pinName;
    return -1; // Invalid index
}
#endif