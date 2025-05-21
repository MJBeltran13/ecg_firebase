import { StyleSheet } from 'react-native';
import { ThemedView } from '@/components/ThemedView';
import SimpleDashboard from '@/components/SimpleDashboard';
import PatientInfoScreen from '@/components/PatientInfoScreen';
import { useState } from 'react';

export default function HomeScreen() {
  const [showDashboard, setShowDashboard] = useState(false);

  const handleStartRecording = () => {
    setShowDashboard(true);
  };

  return (
    <ThemedView style={styles.container}>
      {showDashboard ? (
        <SimpleDashboard />
      ) : (
        <PatientInfoScreen onStartRecording={handleStartRecording} />
      )}
    </ThemedView>
  );
}

const styles = StyleSheet.create({
  container: {
    flex: 1,
    padding: 0,
  },
});
