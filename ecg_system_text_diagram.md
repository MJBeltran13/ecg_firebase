# ECG Monitoring System - Text-Based Flowchart

```
┌───────────────────┐
│     Start         │
└─────────┬─────────┘
          ▼
┌───────────────────┐
│ Initialize System │
└─────────┬─────────┘
          ▼
┌───────────────────┐
│ Create RTOS Tasks │
└─────────┬─────────┘
          ├─────────────────┬───────────────────┐
          ▼                 ▼                   ▼
┌───────────────────┐ ┌─────────────┐  ┌────────────────────┐
│  ECG Task         │ │  LED Task   │  │  Other Tasks       │
│ (High Priority)   │ │             │  │ - Battery          │
└─────────┬─────────┘ └─────────────┘  │ - Potentiometer    │
          ▼                            │ - Firebase         │
     ┌────/\────┐                      │ - Display          │
     │  Leads   │                      └────────────────────┘
     │Connected?│
     └────\/────┘
      /       \
     / No      \ Yes
    ▼           ▼
┌─────────┐ ┌──────────────┐
│  Wait   │ │ Read ECG     │
└─────────┘ └──────┬───────┘
                   ▼
          ┌──────────────────────┐
          │ Process Signal:      │
          │ 1. Filter            │
          │ 2. QRS Detection     │
          │ 3. Calculate BPM     │
          └──────────┬───────────┘
                     ▼
                ┌────/\────┐
                │Classify  │
                │Heart Rate│
                └────\/────┘
                /          \
               /            \
              ▼              ▼
     ┌─────────────┐  ┌────────────┐
     │ Maternal HR │  │ Fetal HR   │
     │ (30-120 BPM)│  │(90-220 BPM)│
     └──────┬──────┘  └─────┬──────┘
            │               │
            └───────┬───────┘
                    ▼
          ┌────────────────┐
          │ Update Data    │
          │ Queues         │
          └────────┬───────┘
                   │
          ┌────────┴──────────┐
          │                    │
          ▼                    ▼                    ▼
┌─────────────────┐  ┌──────────────────┐  ┌──────────────────┐
│  Update LEDs    │  │ Send to Firebase │  │ Update Display   │
└────────┬────────┘  └──────────────────┘  └──────────────────┘
         ▼
┌─────────────────┐
│Read Potentiometer│
└────────┬────────┘
         ▼
┌─────────────────┐
│Adjust Sensitivity│
└────────┬────────┘
         ▼
┌─────────────────┐
│Check Battery    │
└────────┬────────┘
         ▼
┌─────────────────┐
│Repeat Main Loop │────────────┐
└─────────────────┘            │
         ▲                     │
         └─────────────────────┘
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

## Hardware Connections

- **AD8232 ECG Sensor**: OUTPUT → Pin 35, LO+ → Pin 33, LO- → Pin 32
- **Potentiometer**: Middle pin → ESP32 pin 14
- **Status LEDs**: 
  - Fetal heartbeat → Pin 25
  - Working indicator → Pin 26
  - WiFi status → Pin 27
- **Battery Monitoring**: Pin 34 