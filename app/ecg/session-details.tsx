import React, { useState, useEffect } from 'react';
import {
  View,
  Text,
  StyleSheet,
  ScrollView,
  Dimensions,
  ActivityIndicator,
  Alert,
  TouchableOpacity,
} from 'react-native';
import { useLocalSearchParams, useRouter } from 'expo-router';
import { getAllEcgSessions, EcgSession, deleteEcgSession } from '../../constants/LocalStorage';
import { LineChart } from 'react-native-chart-kit';
import { FontAwesome } from '@expo/vector-icons';

const SessionDetailsScreen: React.FC = () => {
  const { id } = useLocalSearchParams<{ id: string }>();
  const [session, setSession] = useState<EcgSession | null>(null);
  const [loading, setLoading] = useState(true);
  const router = useRouter();

  useEffect(() => {
    loadSessionData();
  }, [id]);

  const loadSessionData = async () => {
    try {
      const allSessions = await getAllEcgSessions();
      const selectedSession = allSessions.find(s => s.startTime === id);
      if (selectedSession) {
        setSession(selectedSession);
      } else {
        Alert.alert('Error', 'Session not found');
        router.back();
      }
    } catch (error) {
      Alert.alert('Error', 'Failed to load session data');
      router.back();
    } finally {
      setLoading(false);
    }
  };

  const handleDeleteSession = async () => {
    Alert.alert(
      'Delete Session',
      'Are you sure you want to delete this session? This action cannot be undone.',
      [
        { text: 'Cancel', style: 'cancel' },
        {
          text: 'Delete',
          style: 'destructive',
          onPress: async () => {
            try {
              setLoading(true);
              await deleteEcgSession(id);
              router.back();
            } catch (error) {
              console.error('Delete error:', error);
              setLoading(false);
              Alert.alert('Error', 'Failed to delete session');
            }
          },
        },
      ]
    );
  };

  const formatDate = (dateString: string) => {
    return new Date(dateString).toLocaleString();
  };

  if (loading || !session) {
    return (
      <View style={styles.loadingContainer}>
        <ActivityIndicator size="large" color="#0066cc" />
      </View>
    );
  }

  const chartConfig = {
    backgroundColor: '#ffffff',
    backgroundGradientFrom: '#ffffff',
    backgroundGradientTo: '#ffffff',
    decimalPlaces: 0,
    color: (opacity = 1) => `rgba(0, 102, 204, ${opacity})`,
    labelColor: (opacity = 1) => `rgba(0, 0, 0, ${opacity})`,
    propsForLabels: {
      fontSize: 10,
    },
    propsForDots: {
      r: '3',
    },
    style: {
      borderRadius: 16,
    },
    formatYLabel: (value: number) => Math.round(value).toString(),
  };

  const screenWidth = Dimensions.get('window').width;
  const chartWidth = screenWidth - 32; // 16px padding on each side

  const bpmChartData = {
    labels: session.readings.map((_, index) => {
      // Only show every 10th label to prevent overcrowding
      return index % 10 === 0 ? Math.round(index * (session.readings.length / 6)).toString() : '';
    }),
    datasets: [
      {
        data: session.readings.map(r => r.bpm || 0),
        color: () => '#4CAF50',
        strokeWidth: 2,
      },
    ],
  };

  const ecgChartData = {
    labels: session.readings.map((_, index) => {
      // Only show every 10th label to prevent overcrowding
      return index % 10 === 0 ? Math.round(index * (session.readings.length / 6)).toString() : '';
    }),
    datasets: [
      {
        data: session.readings.map(r => r.rawEcg || 0),
        color: () => '#FF6B6B',
        strokeWidth: 2,
      },
    ],
  };

  return (
    <ScrollView style={styles.container}>
      <View style={styles.header}>
        <Text style={styles.title}>Session Details</Text>
        <TouchableOpacity
          style={styles.deleteButton}
          onPress={handleDeleteSession}
          hitSlop={{ top: 10, bottom: 10, left: 10, right: 10 }}
        >
          <FontAwesome name="trash" size={24} color="#ff3b30" />
        </TouchableOpacity>
      </View>

      <View style={styles.infoCard}>
        <Text style={styles.label}>Patient Name</Text>
        <Text style={styles.value}>{session.patientInfo.name}</Text>

        <Text style={styles.label}>Months Pregnant</Text>
        <Text style={styles.value}>{session.patientInfo.monthsPregnant}</Text>

        <Text style={styles.label}>Start Time</Text>
        <Text style={styles.value}>{formatDate(session.startTime)}</Text>

        {session.endTime && (
          <>
            <Text style={styles.label}>End Time</Text>
            <Text style={styles.value}>{formatDate(session.endTime)}</Text>

            <Text style={styles.label}>Duration</Text>
            <Text style={styles.value}>
              {Math.round((new Date(session.endTime).getTime() - new Date(session.startTime).getTime()) / 1000)}s
            </Text>
          </>
        )}
      </View>

      <View style={styles.chartContainer}>
        <Text style={styles.chartTitle}>BPM Over Time</Text>
        <LineChart
          data={bpmChartData}
          width={chartWidth}
          height={200}
          chartConfig={chartConfig}
          bezier
          style={styles.chart}
          withDots={false}
          withShadow={false}
          withVerticalLabels={true}
          withHorizontalLabels={true}
          fromZero={false}
        />
      </View>

      <View style={styles.chartContainer}>
        <Text style={styles.chartTitle}>ECG Waveform</Text>
        <LineChart
          data={ecgChartData}
          width={chartWidth}
          height={200}
          chartConfig={chartConfig}
          bezier
          style={styles.chart}
          withDots={false}
          withShadow={false}
          withVerticalLabels={true}
          withHorizontalLabels={true}
          fromZero={false}
        />
      </View>

      <View style={styles.statsContainer}>
        <Text style={styles.statsTitle}>Recording Statistics</Text>
        <View style={styles.statsGrid}>
          <View style={styles.statItem}>
            <Text style={styles.statLabel}>Total Readings</Text>
            <Text style={styles.statValue}>{session.readings.length}</Text>
          </View>
          <View style={styles.statItem}>
            <Text style={styles.statLabel}>Avg BPM</Text>
            <Text style={styles.statValue}>
              {Math.round(session.readings.reduce((sum, r) => sum + (r.bpm || 0), 0) / session.readings.length)}
            </Text>
          </View>
          <View style={styles.statItem}>
            <Text style={styles.statLabel}>Max BPM</Text>
            <Text style={styles.statValue}>
              {Math.max(...session.readings.map(r => r.bpm || 0))}
            </Text>
          </View>
        </View>
      </View>
    </ScrollView>
  );
};

const styles = StyleSheet.create({
  container: {
    flex: 1,
    backgroundColor: '#f5f5f5',
  },
  loadingContainer: {
    flex: 1,
    justifyContent: 'center',
    alignItems: 'center',
  },
  header: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    padding: 16,
    backgroundColor: '#fff',
    borderBottomWidth: 1,
    borderBottomColor: '#e1e1e1',
  },
  title: {
    fontSize: 24,
    fontWeight: 'bold',
    color: '#333',
  },
  deleteButton: {
    padding: 8,
  },
  infoCard: {
    margin: 16,
    padding: 16,
    backgroundColor: '#fff',
    borderRadius: 12,
    elevation: 2,
    shadowColor: '#000',
    shadowOffset: { width: 0, height: 2 },
    shadowOpacity: 0.1,
    shadowRadius: 4,
  },
  label: {
    fontSize: 14,
    color: '#666',
    marginTop: 8,
  },
  value: {
    fontSize: 16,
    color: '#333',
    marginBottom: 8,
  },
  chartContainer: {
    margin: 16,
    padding: 12,
    backgroundColor: '#fff',
    borderRadius: 12,
    elevation: 2,
    shadowColor: '#000',
    shadowOffset: { width: 0, height: 2 },
    shadowOpacity: 0.1,
    shadowRadius: 4,
    alignItems: 'center',
  },
  chartTitle: {
    fontSize: 18,
    fontWeight: '600',
    color: '#333',
    marginBottom: 16,
    alignSelf: 'flex-start',
  },
  chart: {
    marginVertical: 4,
    borderRadius: 12,
  },
  statsContainer: {
    margin: 16,
    padding: 16,
    backgroundColor: '#fff',
    borderRadius: 12,
    elevation: 2,
    shadowColor: '#000',
    shadowOffset: { width: 0, height: 2 },
    shadowOpacity: 0.1,
    shadowRadius: 4,
  },
  statsTitle: {
    fontSize: 18,
    fontWeight: '600',
    color: '#333',
    marginBottom: 16,
  },
  statsGrid: {
    flexDirection: 'row',
    justifyContent: 'space-around',
    flexWrap: 'wrap',
    gap: 16,
  },
  statItem: {
    alignItems: 'center',
    minWidth: 100,
  },
  statLabel: {
    fontSize: 14,
    color: '#666',
    marginBottom: 4,
  },
  statValue: {
    fontSize: 24,
    fontWeight: '600',
    color: '#333',
  },
});

export default SessionDetailsScreen; 