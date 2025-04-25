import { StyleSheet } from 'react-native';
import { ThemedView } from '@/components/ThemedView';
import ECGMonitor from '@/components/ECGMonitor';

export default function HomeScreen() {
  return (
    <ThemedView style={styles.container}>
      <ECGMonitor />
    </ThemedView>
  );
}

const styles = StyleSheet.create({
  container: {
    flex: 1,
    padding: 20,
  },
});
