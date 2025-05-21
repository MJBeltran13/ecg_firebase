import React, { useState, useEffect } from 'react';
import {
  View,
  Text,
  StyleSheet,
  FlatList,
  TouchableOpacity,
  Alert,
  ActivityIndicator,
} from 'react-native';
import { getAllEcgSessions, EcgSession, clearAllEcgSessions } from '../../constants/LocalStorage';
import { useRouter } from 'expo-router';
import { FontAwesome } from '@expo/vector-icons';

const SavedSessionsScreen: React.FC = () => {
  const [sessions, setSessions] = useState<EcgSession[]>([]);
  const [loading, setLoading] = useState(true);
  const router = useRouter();

  useEffect(() => {
    loadSessions();
  }, []);

  const loadSessions = async () => {
    try {
      const allSessions = await getAllEcgSessions();
      setSessions(allSessions);
    } catch (error) {
      Alert.alert('Error', 'Failed to load saved sessions');
    } finally {
      setLoading(false);
    }
  };

  const handleClearAllSessions = () => {
    Alert.alert(
      'Clear All Sessions',
      'Are you sure you want to delete all saved sessions? This action cannot be undone.',
      [
        { text: 'Cancel', style: 'cancel' },
        {
          text: 'Clear All',
          style: 'destructive',
          onPress: async () => {
            try {
              await clearAllEcgSessions();
              setSessions([]);
            } catch (error) {
              Alert.alert('Error', 'Failed to clear sessions');
            }
          },
        },
      ]
    );
  };

  const formatDate = (dateString: string) => {
    return new Date(dateString).toLocaleString();
  };

  const renderSession = ({ item }: { item: EcgSession }) => (
    <TouchableOpacity
      style={styles.sessionCard}
      onPress={() => router.push(`/ecg/session-details?id=${item.startTime}`)}
    >
      <View style={styles.sessionHeader}>
        <Text style={styles.patientName}>{item.patientInfo.name}</Text>
        <Text style={styles.sessionTime}>{formatDate(item.startTime)}</Text>
      </View>
      <View style={styles.sessionDetails}>
        <Text style={styles.detailText}>
          Months Pregnant: {item.patientInfo.monthsPregnant}
        </Text>
        <Text style={styles.detailText}>
          Duration: {item.endTime ? 
            Math.round((new Date(item.endTime).getTime() - new Date(item.startTime).getTime()) / 1000) + 's' 
            : 'In Progress'}
        </Text>
      </View>
    </TouchableOpacity>
  );

  if (loading) {
    return (
      <View style={styles.loadingContainer}>
        <ActivityIndicator size="large" color="#0066cc" />
      </View>
    );
  }

  return (
    <View style={styles.container}>
      <View style={styles.header}>
        <Text style={styles.title}>Saved Sessions</Text>
        {sessions.length > 0 && (
          <TouchableOpacity onPress={handleClearAllSessions} style={styles.clearButton}>
            <FontAwesome name="trash" size={24} color="#ff3b30" />
          </TouchableOpacity>
        )}
      </View>
      {sessions.length === 0 ? (
        <View style={styles.emptyContainer}>
          <Text style={styles.emptyText}>No saved sessions found</Text>
        </View>
      ) : (
        <FlatList
          data={sessions}
          renderItem={renderSession}
          keyExtractor={(item) => item.startTime}
          contentContainerStyle={styles.listContainer}
        />
      )}
    </View>
  );
};

const styles = StyleSheet.create({
  container: {
    flex: 1,
    backgroundColor: '#f5f5f5',
    padding: 16,
  },
  header: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    marginBottom: 16,
  },
  title: {
    fontSize: 24,
    fontWeight: 'bold',
    color: '#333',
  },
  clearButton: {
    padding: 8,
  },
  loadingContainer: {
    flex: 1,
    justifyContent: 'center',
    alignItems: 'center',
  },
  listContainer: {
    flexGrow: 1,
  },
  sessionCard: {
    backgroundColor: 'white',
    borderRadius: 12,
    padding: 16,
    marginBottom: 12,
    elevation: 2,
    shadowColor: '#000',
    shadowOffset: { width: 0, height: 2 },
    shadowOpacity: 0.1,
    shadowRadius: 4,
  },
  sessionHeader: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    marginBottom: 8,
  },
  patientName: {
    fontSize: 18,
    fontWeight: '600',
    color: '#333',
  },
  sessionTime: {
    fontSize: 14,
    color: '#666',
  },
  sessionDetails: {
    marginTop: 8,
  },
  detailText: {
    fontSize: 14,
    color: '#666',
    marginBottom: 4,
  },
  emptyContainer: {
    flex: 1,
    justifyContent: 'center',
    alignItems: 'center',
  },
  emptyText: {
    fontSize: 16,
    color: '#666',
    textAlign: 'center',
  },
});

export default SavedSessionsScreen; 