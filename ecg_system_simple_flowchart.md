# ECG Monitoring System - Simplified Flowchart

```mermaid
flowchart TD
    start([Start]) --> init[Initialize System]
    init --> tasks[Create FreeRTOS Tasks]
    
    tasks --> ecgTask[ECG Task\nHighest Priority]
    tasks --> ledTask[LED Task]
    tasks --> otherTasks[Other Tasks\n- Battery\n- Potentiometer\n- Firebase\n- Display]
    
    ecgTask --> checkLeads{Leads Connected?}
    checkLeads -->|No| waitLeads[Wait for Connection]
    checkLeads -->|Yes| readECG[Read ECG Signal]
    
    readECG --> processSignal[Process Signal\n1. Filter\n2. QRS Detection\n3. Calculate BPM]
    processSignal --> classifyHR{Classify Heart Rate}
    
    classifyHR -->|30-120 BPM| maternal[Maternal Heart Rate]
    classifyHR -->|90-220 BPM| fetal[Fetal Heart Rate]
    
    maternal --> updateQueues[Update Data Queues]
    fetal --> updateQueues
    
    updateQueues --> ledUpdate[Update LEDs]
    updateQueues --> firebaseUpdate[Send to Firebase]
    updateQueues --> displayUpdate[Update Serial Display]
    
    ledUpdate --> potentiometerRead[Read Potentiometer]
    potentiometerRead --> adjustSensitivity[Adjust ECG Sensitivity]
    
    adjustSensitivity --> checkBattery[Check Battery Status]
    checkBattery --> loop[Repeat Main Loop]
    loop --> checkLeads
    
    %% Add some styling
    classDef process fill:#f9f,stroke:#333,stroke-width:2px;
    classDef decision fill:#bbf,stroke:#333,stroke-width:2px;
    classDef io fill:#bfb,stroke:#333,stroke-width:2px;
    
    class processSignal,updateQueues,adjustSensitivity process;
    class checkLeads,classifyHR decision;
    class readECG,potentiometerRead io;
```

## System Overview

1. **Initialization**
   - ESP32 startup and hardware setup
   - Create RTOS tasks

2. **Main Tasks**
   - **ECG Processing**: Capture and process ECG signal
   - **LED Control**: Visual feedback of system status
   - **Support Tasks**: Battery monitoring, potentiometer reading, data transmission

3. **ECG Signal Flow**
   - Read AD8232 sensor
   - Apply filters to clean the signal
   - Detect QRS complexes using Pan-Tompkins algorithm
   - Calculate and classify heart rates as maternal or fetal

4. **Output Handling**
   - Visual feedback through LEDs
   - Data transmission to Firebase
   - Serial output for debugging

## Hardware Setup

- **ECG Sensor**: AD8232 connected to ESP32
- **User Interface**: Status LEDs and calibration potentiometer
- **Power Management**: Battery monitoring
- **Connectivity**: WiFi for Firebase data transmission 