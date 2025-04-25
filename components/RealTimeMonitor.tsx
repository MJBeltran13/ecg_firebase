import React, { useEffect, useState } from 'react';
import { View, Text, StyleSheet, Dimensions, ScrollView } from 'react-native';

interface RealTimeMonitorProps {
  data: {
    bpm: number;
    ecg: number[];
    timestamp: number;
  }[];
}

const RealTimeMonitor: React.FC<RealTimeMonitorProps> = ({ data }) => {
  const [bpmData, setBpmData] = useState<number[]>([]);
  const [ecgData, setEcgData] = useState<number[]>([]);

  useEffect(() => {
    if (data.length > 0) {
      // Update BPM data
      const newBpmData = data.map(point => point.bpm);
      setBpmData(newBpmData.slice(-20)); // Keep last 20 points

      // Update ECG data
      if (data[data.length - 1].ecg.length > 0) {
        setEcgData(data[data.length - 1].ecg);
      }
    }
  }, [data]);

  const renderBPMChart = () => {
    const { width } = Dimensions.get('window');
    const chartWidth = width - 40;
    const height = 150;
    const maxBpm = Math.max(...bpmData, 100);
    const minBpm = Math.min(...bpmData, 60);

    return (
      <View style={styles.chartContainer}>
        <Text style={styles.chartTitle}>Real-time BPM</Text>
        <View style={[styles.chart, { width: chartWidth, height }]}>
          {bpmData.map((value, index) => {
            const x = (index / (bpmData.length - 1)) * chartWidth;
            const y = height - ((value - minBpm) / (maxBpm - minBpm)) * height;
            return (
              <View
                key={index}
                style={[
                  styles.dataPoint,
                  { left: x, top: y }
                ]}
              />
            );
          })}
        </View>
      </View>
    );
  };

  const renderECGChart = () => {
    const { width } = Dimensions.get('window');
    const chartWidth = width - 40;
    const height = 150;
    const maxEcg = Math.max(...ecgData, 1);
    const minEcg = Math.min(...ecgData, -1);

    return (
      <View style={styles.chartContainer}>
        <Text style={styles.chartTitle}>Real-time ECG Waveform</Text>
        <View style={[styles.chart, { width: chartWidth, height }]}>
          {ecgData.map((value, index) => {
            const x = (index / (ecgData.length - 1)) * chartWidth;
            const y = height - ((value - minEcg) / (maxEcg - minEcg)) * height;
            return (
              <View
                key={index}
                style={[
                  styles.dataPoint,
                  { left: x, top: y }
                ]}
              />
            );
          })}
        </View>
      </View>
    );
  };

  return (
    <ScrollView style={styles.container}>
      {renderBPMChart()}
      {renderECGChart()}
    </ScrollView>
  );
};

const styles = StyleSheet.create({
  container: {
    flex: 1,
  },
  chartContainer: {
    marginBottom: 20,
    backgroundColor: '#ffffff',
    borderRadius: 10,
    padding: 10,
    shadowColor: '#000',
    shadowOffset: {
      width: 0,
      height: 1,
    },
    shadowOpacity: 0.05,
    shadowRadius: 2,
    elevation: 2,
  },
  chartTitle: {
    fontSize: 16,
    fontWeight: '600',
    marginBottom: 10,
    color: '#4A4A4A',
  },
  chart: {
    backgroundColor: '#F7F7F7',
    borderRadius: 8,
    position: 'relative',
    overflow: 'hidden',
  },
  dataPoint: {
    position: 'absolute',
    width: 3,
    height: 3,
    backgroundColor: '#4A90E2',
    borderRadius: 1.5,
  },
});

export default RealTimeMonitor; 