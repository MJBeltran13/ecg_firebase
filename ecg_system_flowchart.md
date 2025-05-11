# ECG Monitoring System Flowchart (FreeRTOS)

```mermaid
flowchart TD
    subgraph "System Initialization"
        start([Start]) --> init[Initialize Serial, Pins, etc.]
        init --> createMutex[Create Mutex for Data Protection]
        createMutex --> createQueues[Create Queues for Data Exchange]
        createQueues --> initBuffers[Initialize Buffers]
        initBuffers --> ledTest[Test LEDs]
        ledTest --> wifiInit[Initialize WiFi]
        wifiInit --> batteryInit[Initial Battery Reading]
        batteryInit --> createTasks[Create FreeRTOS Tasks]
    end

    subgraph "RTOS Tasks"
        createTasks --> ecgT[ECG Task\nCore 1, Priority 3]
        createTasks --> ledT[LED Task\nCore 1, Priority 2]
        createTasks --> batT[Battery Task\nCore 0, Priority 1]
        createTasks --> potT[Potentiometer Task\nCore 0, Priority 2]
        createTasks --> fireT[Firebase Task\nCore 0, Priority 1]
        createTasks --> dispT[Display Task\nCore 0, Priority 1]
    end

    subgraph "ECG Task" 
        ecgT --> checkLeads{Leads Connected?}
        checkLeads -->|No| delay1[Delay]
        checkLeads -->|Yes| readECG[Read AD8232 Output]
        readECG --> filterECG[Filter ECG Signal]
        filterECG --> detectPeaks[Detect Peaks and Calculate BPM]
        detectPeaks --> sendECGData[Send ECG Data to Queue]
        sendECGData --> sendBPMData[Send BPM Data to Queue]
        sendBPMData --> delay1
        delay1 --> checkLeads
    end

    subgraph "Potentiometer Task"
        potT --> readPot[Read Potentiometer Value]
        readPot --> potChanged{Significant Change?}
        potChanged -->|No| delay2[Delay]
        potChanged -->|Yes| updateSensitivity[Update Prominence & Distance]
        updateSensitivity --> reportChange[Report Changes Every Second]
        reportChange --> delay2
        delay2 --> readPot
    end

    subgraph "LED Task"
        ledT --> updateFetalLED[Update Fetal LED Based on Heartbeat]
        updateFetalLED --> updateWorkingLED[Update Working LED Based on Status]
        updateWorkingLED --> updateWiFiLED[Update WiFi LED Based on Connection]
        updateWiFiLED --> delay3[Delay]
        delay3 --> updateFetalLED
    end

    subgraph "Battery Task"
        batT --> readBattery[Read Battery Voltage]
        readBattery --> reportBattery[Report Battery Status]
        reportBattery --> delay4[Delay 5 seconds]
        delay4 --> readBattery
    end

    subgraph "Firebase Task"
        fireT --> getBPMData[Get Latest BPM Data from Queue]
        getBPMData --> getBPMValid{Valid BPM?}
        getBPMValid -->|No| delay5[Delay]
        getBPMValid -->|Yes| getECGData[Get Latest ECG Data from Queue]
        getECGData --> sendToFirebase[Send Data to Firebase]
        sendToFirebase --> delay5
        delay5 --> getBPMData
    end

    subgraph "Display Task"
        dispT --> getBatteryStatus[Get Battery Status]
        getBatteryStatus --> getWiFiStatus[Get WiFi Status]
        getWiFiStatus --> getBPMInfo[Get BPM Information]
        getBPMInfo --> getPotValue[Get Potentiometer Value]
        getPotValue --> getSignalQuality[Get Signal Quality]
        getSignalQuality --> printInfo[Print Info to Serial]
        printInfo --> delay6[Delay 1 second]
        delay6 --> getBatteryStatus
    end

    subgraph "Signal Processing"
        filterECG --> normalFilter[Weighted Moving Average Filter]
        normalFilter --> removeBaseline[Remove Baseline Wander]
        removeBaseline --> panTompkins[Pan-Tompkins QRS Detection]
        panTompkins --> detectPeaks
    end

    subgraph "Pan-Tompkins Algorithm"
        panTompkins --> bandpassFilter[Bandpass Filter]
        bandpassFilter --> derivative[Derivative]
        derivative --> squaring[Squaring]
        squaring --> movingWindow[Moving Window Integration]
        movingWindow --> adaptiveThreshold[Adaptive Thresholding]
        adaptiveThreshold --> qrsDetection{QRS Detected?}
        qrsDetection -->|Yes| updatePeak[Update Peak Data]
        qrsDetection -->|No| checkNoise[Check for Noise]
    end

    subgraph "BPM Classification" 
        detectPeaks --> calcInterval[Calculate R-R Interval]
        calcInterval --> calculateBPM[Calculate BPM]
        calculateBPM --> validateBPM{Valid BPM?}
        validateBPM -->|No| end1[End]
        validateBPM -->|Yes| classifyBPM{Classify BPM}
        classifyBPM -->|30-120| setMaternal[Set as Maternal BPM]
        classifyBPM -->|90-220| setFetal[Set as Fetal BPM]
    end

    %% Data flow between tasks
    ecgT -.ECG Data.-> fireT
    ecgT -.BPM Data.-> ledT
    ecgT -.BPM Data.-> dispT
    potT -.Sensitivity Settings.-> ecgT
    batT -.Battery Status.-> ledT
    batT -.Battery Status.-> dispT
```

## Main System Components

1. **Initialization System**
   - Sets up hardware, mutexes, queues, and creates RTOS tasks

2. **RTOS Tasks**
   - **ECG Task (Highest Priority)**: Reads and processes ECG signal, detects peaks, calculates BPM
   - **LED Task**: Updates status LEDs based on heartbeat, device operation, and WiFi status
   - **Battery Task**: Monitors battery voltage
   - **Potentiometer Task**: Reads potentiometer to adjust ECG sensitivity settings
   - **Firebase Task**: Sends ECG and BPM data to Firebase
   - **Display Task**: Outputs status information to serial monitor

3. **Signal Processing Pipeline**
   - Filtering: Weighted moving average and baseline removal
   - Pan-Tompkins QRS Detection Algorithm
   - Peak detection and BPM calculation
   - Heart rate classification (maternal vs. fetal)

4. **Inter-task Communication**
   - Uses queues for data exchange
   - Uses mutexes for safe access to shared data

## Hardware Connections

- **AD8232 ECG Sensor**: OUTPUT → Pin 35, LO+ → Pin 33, LO- → Pin 32
- **Potentiometer**: Middle pin → ESP32 pin 14
- **Status LEDs**: 
  - Fetal heartbeat → Pin 25
  - Working indicator → Pin 26
  - WiFi status → Pin 27
- **Battery Monitoring**: Pin 34 