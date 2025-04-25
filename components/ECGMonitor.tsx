import React, { useEffect, useState } from 'react';
import { View, Text, StyleSheet, useColorScheme } from 'react-native';
import RealTimeMonitor from './RealTimeMonitor';

const ECGMonitor: React.FC = () => {
  const [monitorData, setMonitorData] = useState<{
    bpm: number;
    ecg: number[];
    timestamp: number;
  }[]>([]);

  const colorScheme = useColorScheme();
  const isDark = colorScheme === 'dark';

  useEffect(() => {
    const interval = setInterval(() => {
      // Generate random BPM between 60-100
      const bpm = Math.floor(Math.random() * 40) + 60;
      
      // Generate ECG waveform data
      const ecg = Array.from({ length: 100 }, (_, i) => {
        // Basic ECG waveform simulation
        const t = i / 10;
        return Math.sin(t) * 0.5 + 
               Math.sin(2 * t) * 0.2 + 
               Math.sin(3 * t) * 0.1 +
               (Math.random() - 0.5) * 0.1; // Add some noise
      });

      const newData = {
        bpm,
        ecg,
        timestamp: Date.now()
      };

      setMonitorData(prevData => {
        // Keep only the last 100 data points
        const updatedData = [...prevData, newData].slice(-100);
        return updatedData;
      });
    }, 1000); // Update every second

    return () => clearInterval(interval);
  }, []);

  return (
    <View style={[styles.container, isDark && styles.containerDark]}>
      <Text style={[styles.title, isDark && styles.titleDark]}>ECG Monitor</Text>
      <View style={[styles.bpmContainer, isDark && styles.bpmContainerDark]}>
        <Text style={[styles.bpmLabel, isDark && styles.textDark]}>Current BPM:</Text>
        <Text style={[styles.bpmValue, isDark && styles.textDark]}>
          {monitorData.length > 0 ? monitorData[monitorData.length - 1].bpm : '--'}
        </Text>
      </View>
      <RealTimeMonitor data={monitorData} />
    </View>
  );
};

const styles = StyleSheet.create({
  container: {
    flex: 1,
    padding: 16,
    backgroundColor: '#F9F9F9',
  },
  containerDark: {
    backgroundColor: '#1c1c1e',
  },
  title: {
    fontSize: 24,
    fontWeight: '600',
    marginBottom: 16,
    color: '#4A4A4A',
  },
  titleDark: {
    color: '#fff',
  },
  bpmContainer: {
    flexDirection: 'row',
    alignItems: 'center',
    backgroundColor: '#FFFFFF',
    padding: 12,
    borderRadius: 8,
    marginBottom: 16,
    shadowColor: '#000',
    shadowOffset: {
      width: 0,
      height: 1,
    },
    shadowOpacity: 0.05,
    shadowRadius: 2,
    elevation: 2,
  },
  bpmContainerDark: {
    backgroundColor: '#2c2c2e',
  },
  bpmLabel: {
    fontSize: 16,
    marginRight: 8,
    color: '#4A4A4A',
  },
  bpmValue: {
    fontSize: 20,
    fontWeight: '600',
    color: '#4A90E2',
  },
  textDark: {
    color: '#fff',
  },
});

export default ECGMonitor; 