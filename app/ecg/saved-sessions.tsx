import React, { useState, useEffect, useCallback } from 'react';
import {
  View,
  Text,
  StyleSheet,
  FlatList,
  TouchableOpacity,
  Alert,
  ActivityIndicator,
  RefreshControl,
} from 'react-native';
import { getAllEcgSessions, EcgSession, clearAllEcgSessions, deleteEcgSession } from '../../constants/LocalStorage';
import { useRouter } from 'expo-router';
import { FontAwesome } from '@expo/vector-icons';

const SavedSessionsScreen: React.FC = () => {
  const [sessions, setSessions] = useState<EcgSession[]>([]);
  const [loading, setLoading] = useState(true);
  const [refreshing, setRefreshing] = useState(false);
  const router = useRouter();

  const loadSessions = useCallback(async () => {
    try {
      console.log('Loading sessions...');
      const allSessions = await getAllEcgSessions();
      console.log('Loaded sessions:', allSessions.length);
      setSessions(allSessions);
    } catch (error) {
      console.error('Error loading sessions:', error);
      Alert.alert('Error', 'Failed to load saved sessions');
    } finally {
      setLoading(false);
      setRefreshing(false);
    }
  }, []);

  useEffect(() => {
    loadSessions();
  }, [loadSessions]);

  const handleRefresh = useCallback(async () => {
    setRefreshing(true);
    await loadSessions();
  }, [loadSessions]);

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

  const handleDeleteSession = useCallback((sessionStartTime: string) => {
    console.log('Delete button pressed for session:', sessionStartTime);
    Alert.alert(
      'Delete Session',
      'Are you sure you want to delete this session? This action cannot be undone.',
      [
        { text: 'Cancel', style: 'cancel' },
        {
          text: 'Delete',
          style: 'destructive',
          onPress: async () => {
            setLoading(true);
            try {
              console.log('Attempting to delete session:', sessionStartTime);
              await deleteEcgSession(sessionStartTime);
              console.log('Session deleted successfully, reloading sessions...');
              setSessions(prevSessions => 
                prevSessions.filter(session => session.startTime !== sessionStartTime)
              );
              await loadSessions();
            } catch (error) {
              console.error('Delete error:', error);
              Alert.alert('Error', 'Failed to delete session');
            } finally {
              setLoading(false);
            }
          },
        },
      ]
    );
  }, [loadSessions]);

  const formatDate = (dateString: string) => {
    return new Date(dateString).toLocaleString();
  };

  const renderSession = ({ item }: { item: EcgSession }) => (
    <View style={styles.sessionCard}>
      <TouchableOpacity
        style={styles.sessionContent}
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
      <TouchableOpacity
        style={styles.deleteButton}
        onPress={() => handleDeleteSession(item.startTime)}
        hitSlop={{ top: 10, bottom: 10, left: 10, right: 10 }}
        activeOpacity={0.6}
      >
        <FontAwesome name="trash-o" size={24} color="#ff3b30" />
      </TouchableOpacity>
    </View>
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
        {sessions.length > 0}
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
          refreshControl={
            <RefreshControl 
              refreshing={refreshing} 
              onRefresh={handleRefresh}
              colors={['#0066cc']}
              tintColor="#0066cc"
            />
          }
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
    flexDirection: 'row',
    alignItems: 'center',
  },
  sessionContent: {
    flex: 1,
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
  deleteButton: {
    padding: 12,
    marginLeft: 8,
    justifyContent: 'center',
    alignItems: 'center',
  },
});

export default SavedSessionsScreen; 