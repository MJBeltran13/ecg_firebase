import React, { useState, useEffect } from 'react';
import {
  View,
  Text,
  StyleSheet,
  ScrollView,
  Dimensions,
  ActivityIndicator,
  Alert,
} from 'react-native';
import { useLocalSearchParams, useRouter } from 'expo-router';
import { getAllEcgSessions, EcgSession } from '../../constants/LocalStorage';
import { LineChart } from 'react-native-chart-kit';

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

  const chartData = {
    labels: session.readings.map((_, index) => 
      Math.round(index * (session.readings.length / 6))),
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
        <Text style={styles.chartTitle}>ECG Recording</Text>
        <LineChart
          data={chartData}
          width={Dimensions.get('window').width - 32}
          height={220}
          chartConfig={{
            backgroundColor: '#ffffff',
            backgroundGradientFrom: '#ffffff',
            backgroundGradientTo: '#ffffff',
            decimalPlaces: 0,
            color: (opacity = 1) => `rgba(0, 102, 204, ${opacity})`,
            style: {
              borderRadius: 16,
            },
          }}
          bezier
          style={styles.chart}
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
    padding: 16,
    backgroundColor: '#fff',
    borderRadius: 12,
    elevation: 2,
    shadowColor: '#000',
    shadowOffset: { width: 0, height: 2 },
    shadowOpacity: 0.1,
    shadowRadius: 4,
  },
  chartTitle: {
    fontSize: 18,
    fontWeight: '600',
    color: '#333',
    marginBottom: 16,
  },
  chart: {
    marginVertical: 8,
    borderRadius: 16,
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
  },
  statItem: {
    alignItems: 'center',
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