#include "MachineInterface.h"
#include <QDateTime>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QRandomGenerator>

// Initialize static members PROPERLY
#ifdef GPIO_AVAILABLE
int MachineInterface::ads1_handle = -1;
int MachineInterface::ads2_handle = -1;
#endif

// Constructor
MachineInterface::MachineInterface(QObject* parent) 
    : QObject(parent)
    , currentState(IDLE)
    , gameActive(false)
    , ballDetectionTimer(new QTimer(this))
    , machineTimer(new QTimer(this))
    , detectionActive(false)
    , detectionSuspended(false)
    , ballDetectionCounter(0)
    , detectionThreshold(10)
    , debounceTimeMs(500)
    , currentPinStates({1,1,1,1,1}) 
    , targetPinStates({1,1,1,1,1})
    , machineInOperation(false)
    , machineCycleTimeSeconds(8.5)
    , laneId(1)
{
    // Setup timers
    ballDetectionTimer->setInterval(1); // 1ms for fast ball detection
    machineTimer->setInterval(50);      // 50ms for machine operations
    
    connect(ballDetectionTimer, &QTimer::timeout, this, &MachineInterface::onBallDetectionTimer);
    connect(machineTimer, &QTimer::timeout, this, &MachineInterface::onMachineTimer);
    
    qDebug() << "MachineInterface created";
}

// Destructor
MachineInterface::~MachineInterface() {
    shutdown();
}


// Initialize the machine interface
bool MachineInterface::initialize() {
    qDebug() << "Initializing MachineInterface";
    
    // Load settings first
    loadSettings();
    
#ifdef GPIO_AVAILABLE
    // Initialize wiringPi
    if (wiringPiSetupGpio() < 0) {
        qCritical() << "Failed to setup GPIO";
        emit machineError("GPIO initialization failed");
        return false;
    }
    
    // Setup GPIO pins
    if (!setupGPIO()) {
        qCritical() << "Failed to setup GPIO pins";
        emit machineError("GPIO pin setup failed");
        return false;
    }
    
    // Setup ADS converters
    if (!setupADS()) {
        qWarning() << "Failed to setup ADS converters - pin detection may not work";
        emit machineError("ADS converter setup failed");
        // Don't return false - we can still do basic operations
    }
    
    qDebug() << "Hardware initialized successfully for lane" << laneId;
#else
    qDebug() << "Running in simulation mode (no GPIO) for lane" << laneId;
#endif
    
    // Start machine timer
    machineTimer->start();
    
    emit machineReady();
    return true;
}

// Load settings from settings.json
void MachineInterface::loadSettings() {
    QFile settingsFile("settings.json");
    if (!settingsFile.open(QIODevice::ReadOnly)) {
        qWarning() << "Cannot open settings.json, using defaults";
        laneId = 1;
        gp1 = 12; gp2 = 16; gp3 = 20; gp4 = 21; gp5 = 26; gp6 = 19; gp7 = 13; gp8 = 6;
        return;
    }
    
    QJsonDocument doc = QJsonDocument::fromJson(settingsFile.readAll());
    QJsonObject settings = doc.object();
    
    laneId = settings["Lane"].toInt(1);
    QString laneKey = QString::number(laneId);
    
    if (settings.contains(laneKey)) {
        laneSettings = settings[laneKey].toObject();
        
        // Extract GPIO pins
        gp1 = laneSettings["GP1"].toString().toInt();
        gp2 = laneSettings["GP2"].toString().toInt();
        gp3 = laneSettings["GP3"].toString().toInt();
        gp4 = laneSettings["GP4"].toString().toInt();
        gp5 = laneSettings["GP5"].toString().toInt();
        gp6 = laneSettings["GP6"].toString().toInt();
        gp7 = laneSettings["GP7"].toString().toInt();
        gp8 = laneSettings["GP8"].toString().toInt();
        
        // Pin sensor mappings
        pb10 = laneSettings["B10"].toString();
        pb11 = laneSettings["B11"].toString();
        pb12 = laneSettings["B12"].toString();
        pb13 = laneSettings["B13"].toString();
        pb20 = laneSettings["B20"].toString();
        
        qDebug() << "Loaded settings for lane" << laneId;
        qDebug() << "GPIO pins:" << gp1 << gp2 << gp3 << gp4 << gp5 << gp6 << gp7 << gp8;
    } else {
        qWarning() << "No settings found for lane" << laneId;
        // Use defaults
        gp1 = 12; gp2 = 16; gp3 = 20; gp4 = 21; gp5 = 26; gp6 = 19; gp7 = 13; gp8 = 6;
    }
}

// Setup GPIO pins
bool MachineInterface::setupGPIO() {
#ifdef GPIO_AVAILABLE
    try {
        // Setup output pins (solenoids + reset)
        pinMode(gp1, OUTPUT);  // lTwo solenoid
        pinMode(gp2, OUTPUT);  // lThree solenoid
        pinMode(gp3, OUTPUT);  // cFive solenoid
        pinMode(gp4, OUTPUT);  // rThree solenoid
        pinMode(gp5, OUTPUT);  // rTwo solenoid
        pinMode(gp6, OUTPUT);  // Reset pin
        
        // Setup input pins
        pinMode(gp7, INPUT);   // Ball detection sensor
        pinMode(gp8, INPUT);   // Other sensor
        pullUpDnControl(gp7, PUD_DOWN);
        pullUpDnControl(gp8, PUD_DOWN);
        
        // Set safe initial state - all solenoids OFF (HIGH)
        digitalWrite(gp1, HIGH);
        digitalWrite(gp2, HIGH);
        digitalWrite(gp3, HIGH);
        digitalWrite(gp4, HIGH);
        digitalWrite(gp5, HIGH);
        digitalWrite(gp6, HIGH);
        
        qDebug() << "GPIO pins configured successfully";
        return true;
        
    } catch (...) {
        qCritical() << "Exception during GPIO setup";
        return false;
    }
#endif
    return true; // Simulation mode always succeeds
}

// Setup ADS1115 I2C converters
bool MachineInterface::setupADS() {
#ifdef GPIO_AVAILABLE
    try {
        // Initialize I2C communication with ADS1115 chips
        ads1_handle = wiringPiI2CSetup(0x48); // First ADS1115
        ads2_handle = wiringPiI2CSetup(0x49); // Second ADS1115
        
        if (ads1_handle < 0 || ads2_handle < 0) {
            qCritical() << "Failed to initialize ADS1115 chips";
            return false;
        }
        
        qDebug() << "ADS1115 chips initialized successfully";
        return true;
        
    } catch (...) {
        qCritical() << "Exception during ADS setup";
        return false;
    }
#endif
    return true; // Simulation mode
}

// Start ball detection
void MachineInterface::startBallDetection() {
    qDebug() << "Starting ball detection for lane" << laneId;
    detectionActive = true;
    detectionSuspended = false;
    ballDetectionCounter = 0;
    lastDetectionTime = 0;
    ballDetectionTimer->start();
}

// Stop ball detection
void MachineInterface::stopBallDetection() {
    qDebug() << "Stopping ball detection for lane" << laneId;
    detectionActive = false;
    ballDetectionTimer->stop();
}

// Suspend/resume ball detection
void MachineInterface::setDetectionSuspended(bool suspended) {
    detectionSuspended = suspended;
    qDebug() << "Ball detection" << (suspended ? "suspended" : "resumed") << "for lane" << laneId;
}

// Ball detection timer callback
void MachineInterface::onBallDetectionTimer() {
    if (!detectionActive || detectionSuspended) return;
    
    checkBallSensor();
}

// Check ball sensor and process detection
void MachineInterface::checkBallSensor() {
    // CRITICAL: Only detect balls when machine is idle and game is active
    if (!detectionActive || detectionSuspended || !gameActive || currentState != IDLE) {
        return; // Don't process ball detection if machine is busy
    }

#ifdef GPIO_AVAILABLE
    if (digitalRead(gp7) == HIGH) {
        ballDetectionCounter++;
        
        if (ballDetectionCounter >= detectionThreshold) {
            qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
            if (currentTime - lastDetectionTime >= debounceTimeMs) {
                
                qDebug() << "BALL DETECTED on lane" << laneId;
                lastDetectionTime = currentTime;
                
                QVector<int> detectedStates = readPinSensors();
                currentPinStates = detectedStates;
                
                emit ballDetected(detectedStates);
                emit pinStatesChanged(detectedStates);
            }
            ballDetectionCounter = 0;
        }
    } else {
        ballDetectionCounter = 0;
    }
#else
    // Simulation mode - generate test ball detection
    static int simCounter = 0;
    simCounter++;
    
    if (simCounter > 3000) { // Every 3 seconds in simulation
        simCounter = 0;
        
        // Generate realistic Canadian 5-pin results
        QVector<int> simResults;
        int scenario = QRandomGenerator::global()->bounded(0, 10);
        
        if (scenario == 0) {
            // Strike - all pins down
            simResults = {0, 0, 0, 0, 0};
        } else if (scenario == 1) {
            // Center pin only
            simResults = {1, 1, 0, 1, 1};
        } else if (scenario == 2) {
            // Left side
            simResults = {0, 0, 1, 1, 1};
        } else if (scenario == 3) {
            // Right side
            simResults = {1, 1, 1, 0, 0};
        } else if (scenario == 4) {
            // Split - corners only
            simResults = {0, 1, 1, 1, 0};
        } else {
            // Random combination
            simResults = {1, 1, 1, 1, 1};
            int pinsDown = QRandomGenerator::global()->bounded(1, 4);
            for (int i = 0; i < pinsDown; ++i) {
                int pin = QRandomGenerator::global()->bounded(0, 5);
                simResults[pin] = 0;
            }
        }
        
        currentPinStates = simResults;
        
        qDebug() << "SIMULATED BALL DETECTED on lane" << laneId << "- Pin states:" << simResults;
        emit ballDetected(simResults);
        emit pinStatesChanged(simResults);
    }
#endif
}

// Read pin sensors from ADS converters
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
            qCritical() << "FAILED to read sensor" << sensor.name << "after" << attempts << "attempts, using default PIN UP";
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

// Machine timer callback
void MachineInterface::onMachineTimer() {
    if (!machineInOperation) return;
    
    qint64 elapsed = QDateTime::currentMSecsSinceEpoch() - resetStartTime;
    
    // Wait for machine cycle timing
    if (elapsed >= (machineCycleTimeSeconds * 1000)) {
        // Execute the target pin configuration
        executePinConfiguration(targetPinStates);
        
        currentPinStates = targetPinStates;
        machineInOperation = false;
        
        // CRITICAL: Return to idle state after operation completes
        currentState = IDLE;
        
        emit pinStatesChanged(currentPinStates);
        qDebug() << "Machine cycle complete, pin states:" << currentPinStates;
    }
}

// Reset all pins to UP position
void MachineInterface::resetPins(bool immediate) {
    qDebug() << "Resetting pins to UP position, immediate:" << immediate << "on lane" << laneId;
    
    // Set machine state to prevent ball detection during reset
    currentState = RESETTING;
    
    targetPinStates = {1, 1, 1, 1, 1}; // All pins up
    
    if (immediate) {
        executePinReset();
        currentPinStates = targetPinStates;
        currentState = IDLE; // Return to idle state
        emit pinStatesChanged(currentPinStates);
    } else {
        // Start machine cycle
        machineInOperation = true;
        resetStartTime = QDateTime::currentMSecsSinceEpoch();
        qDebug() << "Machine cycle started for pin reset";
    }
}

void MachineInterface::setPinConfiguration(const QVector<int>& pinStates) {
    qDebug() << "Setting pin configuration:" << pinStates << "on lane" << laneId;
    
    if (pinStates.size() != 5) {
        qWarning() << "Invalid pin configuration size:" << pinStates.size() << "expected 5";
        emit machineError("Invalid pin configuration");
        return;
    }
    
    // Set machine state to prevent ball detection during pin setting
    currentState = SETTING_PINS;
    
    targetPinStates = pinStates;
    
    // Start machine cycle
    machineInOperation = true;
    resetStartTime = QDateTime::currentMSecsSinceEpoch();
    qDebug() << "Machine cycle started for pin configuration";
}

// Execute immediate pin reset
void MachineInterface::executePinReset() {
#ifdef GPIO_AVAILABLE
    try {
        qDebug() << "Executing physical pin reset on lane" << laneId;
        
        // Send reset pulse
        digitalWrite(gp6, LOW);
        delay(50); // 50ms pulse
        digitalWrite(gp6, HIGH);
        
        qDebug() << "Pin reset complete - all pins UP";
        
    } catch (...) {
        qWarning() << "Error during pin reset";
        emit machineError("Pin reset failed");
    }
#else
    qDebug() << "Simulated pin reset complete on lane" << laneId;
#endif
}

// Execute pin configuration
void MachineInterface::executePinConfiguration(const QVector<int>& states) {
#ifdef GPIO_AVAILABLE
    try {
        qDebug() << "Executing pin configuration:" << states << "on lane" << laneId;
        
        // Send reset pulse first
        digitalWrite(gp6, LOW);
        delay(50);
        digitalWrite(gp6, HIGH);
        
        // Wait for B21 sensor or timeout
        if (!waitForB21Sensor(8000)) {
            qWarning() << "B21 sensor timeout, proceeding anyway";
        }
        
        // Apply pin configuration
        QVector<int> gpios = {gp1, gp2, gp3, gp4, gp5};
        
        for (int i = 0; i < 5; ++i) {
            if (states[i] == 0) { // Pin should be down
                qDebug() << "Knocking down pin" << i;
                digitalWrite(gpios[i], LOW);  // Activate solenoid
                delay(150);                   // Hold pulse
                digitalWrite(gpios[i], HIGH); // Deactivate
                delay(50);                    // Brief pause between pins
            }
        }
        
        qDebug() << "Pin configuration applied successfully";
        
    } catch (...) {
        qWarning() << "Error during pin configuration";
        emit machineError("Pin configuration failed");
        
        // Emergency safe state
        digitalWrite(gp1, HIGH);
        digitalWrite(gp2, HIGH);
        digitalWrite(gp3, HIGH);
        digitalWrite(gp4, HIGH);
        digitalWrite(gp5, HIGH);
    }
#else
    qDebug() << "Simulated pin configuration applied:" << states << "on lane" << laneId;
#endif
}

// Wait for B21 sensor (machine timing sensor)
bool MachineInterface::waitForB21Sensor(int timeoutMs) {
#ifdef GPIO_AVAILABLE
    // This would read from your B21 sensor via ADS1115
    // For now, just simulate the wait time
    delay(5500); // Standard machine timing
    return true;
#else
    // Simulate timing
    QTimer::singleShot(100, [](){}); // Brief simulated wait
    return true;
#endif
}

// Set Machine control state
void MachineInterface::setGameActive(bool active) {
    gameActive = active;
    if (!active) {
        currentState = IDLE; // Ensure we're idle when game stops
    }
    qDebug() << "Machine interface game state:" << (active ? "active" : "inactive");
}

// Shutdown the machine interface
void MachineInterface::shutdown() {
#ifdef GPIO_AVAILABLE
    try {
        qDebug() << "Shutting down machine interface for lane" << laneId;
        
        // Stop all timers
        if (ballDetectionTimer) ballDetectionTimer->stop();
        if (machineTimer) machineTimer->stop();
        
        // Set all outputs to safe state
        digitalWrite(gp1, HIGH);
        digitalWrite(gp2, HIGH);
        digitalWrite(gp3, HIGH);
        digitalWrite(gp4, HIGH);
        digitalWrite(gp5, HIGH);
        digitalWrite(gp6, HIGH);
        
        qDebug() << "Machine interface shutdown complete";
        
    } catch (...) {
        qWarning() << "Error during machine interface shutdown";
    }
#else
    qDebug() << "Simulated machine interface shutdown for lane" << laneId;
    if (ballDetectionTimer) ballDetectionTimer->stop();
    if (machineTimer) machineTimer->stop();
#endif
}