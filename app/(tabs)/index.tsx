import { StyleSheet } from 'react-native';
import { ThemedView } from '@/components/ThemedView';
import SimpleDashboard from '@/components/SimpleDashboard';

export default function HomeScreen() {
  return (
    <ThemedView style={styles.container}>
      <SimpleDashboard />
    </ThemedView>
  );
}

const styles = StyleSheet.create({
  container: {
    flex: 1,
    padding: 0,
  },
});
