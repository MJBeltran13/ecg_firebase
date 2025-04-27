import { StyleSheet, View, Image, Platform, ScrollView } from 'react-native';
import { ThemedText } from '@/components/ThemedText';
import { ThemedView } from '@/components/ThemedView';
import { IconSymbol } from '@/components/ui/IconSymbol';

export default function HowToUseScreen() {
  return (
    <ScrollView contentContainerStyle={styles.container}>
     
        <ThemedText style={styles.mainTitle} type="title">How to Use</ThemedText>
    
      
      <View style={styles.cardsContainer}>
        <ThemedView style={styles.card}>
          <View style={styles.cardHeader}>
            <IconSymbol name="cable.connector" size={20} color="#4F86F7" style={styles.cardIcon} />
            <ThemedText style={styles.cardTitle}>Hardware Setup</ThemedText>
          </View>
          <View style={styles.cardContent}>
            <View style={styles.bulletItem}>
              <ThemedText style={styles.bulletPoint}>1</ThemedText>
              <ThemedText style={styles.bulletText}>Connect AD8232 to Yakap Device:</ThemedText>
            </View>
           
          </View>
        </ThemedView>

        <ThemedView style={styles.card}>
          <View style={styles.cardHeader}>
            <IconSymbol name="heart.circle" size={20} color="#F87171" style={styles.cardIcon} />
            <ThemedText style={styles.cardTitle}>Electrode Placement</ThemedText>
          </View>
          <View style={styles.cardContent}>
            <View style={styles.bulletItem}>
              <ThemedText style={styles.bulletPoint}>1</ThemedText>
              <ThemedText style={styles.bulletText}>Red electrode: Right side next to the navel</ThemedText>
            </View>
            <View style={styles.bulletItem}>
              <ThemedText style={styles.bulletPoint}>2</ThemedText>
              <ThemedText style={styles.bulletText}>Yellow electrode: Left side next to the navel</ThemedText>
            </View>
            <View style={styles.bulletItem}>
              <ThemedText style={styles.bulletPoint}>3</ThemedText>
              <ThemedText style={styles.bulletText}>Green electrode: Bottom part below the navel</ThemedText>
            </View>
            <View style={styles.bulletItem}>
              <ThemedText style={styles.bulletPoint}>!</ThemedText>
              <ThemedText style={styles.bulletText}>Clean the skin with alcohol and ensure electrodes make good contact</ThemedText>
            </View>
          </View>
        </ThemedView>

        <ThemedView style={styles.card}>
          <View style={styles.cardHeader}>
            <IconSymbol name="power" size={20} color="#34D399" style={styles.cardIcon} />
            <ThemedText style={styles.cardTitle}>Power On</ThemedText>
          </View>
          <View style={styles.cardContent}>
            <View style={styles.bulletItem}>
              <ThemedText style={styles.bulletPoint}>1</ThemedText>
              <ThemedText style={styles.bulletText}>Power on Device using USB or battery</ThemedText>
            </View>
            <View style={styles.bulletItem}>
              <ThemedText style={styles.bulletPoint}>2</ThemedText>
              <ThemedText style={styles.bulletText}>Wait for Green LED to start blinking (Device ready)</ThemedText>
            </View>
          </View>
        </ThemedView>

        <ThemedView style={styles.card}>
          <View style={styles.cardHeader}>
            <IconSymbol name="waveform.path.ecg" size={20} color="#A78BFA" style={styles.cardIcon} />
            <ThemedText style={styles.cardTitle}>Monitor Display</ThemedText>
          </View>
          <View style={styles.cardContent}>
            <View style={styles.bulletItem}>
              <ThemedText style={styles.bulletPoint}>•</ThemedText>
              <ThemedText style={styles.bulletText}>Top: Real-time heart rate (BPM)</ThemedText>
            </View>
            <View style={styles.bulletItem}>
              <ThemedText style={styles.bulletPoint}>•</ThemedText>
              <ThemedText style={styles.bulletText}>Middle: Live ECG waveform</ThemedText>
            </View>
            <View style={styles.bulletItem}>
              <ThemedText style={styles.bulletPoint}>•</ThemedText>
              <ThemedText style={styles.bulletText}>Bottom: Signal quality indicator</ThemedText>
            </View>
            <View style={styles.bulletItem}>
              <ThemedText style={styles.bulletPoint}>!</ThemedText>
              <ThemedText style={styles.bulletText}>Red indicators show if leads are disconnected</ThemedText>
            </View>
          </View>
        </ThemedView>

        <ThemedView style={styles.card}>
          <View style={styles.cardHeader}>
            <IconSymbol name="exclamationmark.triangle" size={20} color="#F97316" style={styles.cardIcon} />
            <ThemedText style={styles.cardTitle}>Troubleshooting</ThemedText>
          </View>
          <View style={styles.cardContent}>
            <View style={styles.bulletItem}>
              <ThemedText style={styles.bulletPoint}>•</ThemedText>
              <ThemedText style={styles.bulletText}>No signal: Check electrode connections</ThemedText>
            </View>
            <View style={styles.bulletItem}>
              <ThemedText style={styles.bulletPoint}>•</ThemedText>
              <ThemedText style={styles.bulletText}>Noisy signal: Stay still, check electrode contact</ThemedText>
            </View>
            <View style={styles.bulletItem}>
              <ThemedText style={styles.bulletPoint}>•</ThemedText>
              <ThemedText style={styles.bulletText}>Device not found: Reset Device, check power</ThemedText>
            </View>
          
          </View>
        </ThemedView>

        <ThemedView style={styles.card}>
          <View style={styles.cardHeader}>
            <IconSymbol name="dot.radiowaves.left.and.right" size={20} color="#60A5FA" style={styles.cardIcon} />
            <ThemedText style={styles.cardTitle}>Connection Status</ThemedText>
          </View>
          <View style={styles.cardContent}>
            <View style={styles.bulletItem}>
              <ThemedText style={styles.bulletPoint}>•</ThemedText>
              <ThemedText style={styles.bulletText}>Green LED Solid: Device connected and working properly</ThemedText>
            </View>
            <View style={styles.bulletItem}>
              <ThemedText style={styles.bulletPoint}>•</ThemedText>
              <ThemedText style={styles.bulletText}>Red LED Blinking: Device Battery Low</ThemedText>
            </View>
            <View style={styles.bulletItem}>
              <ThemedText style={styles.bulletPoint}>•</ThemedText>
              <ThemedText style={styles.bulletText}>Yellow LED Blinking: Connection lost or Not Connected</ThemedText>
            </View>
          </View>
        </ThemedView>
      </View>
      
    
        <ThemedText style={styles.footerText}>Yakap App v2.0</ThemedText>
     
    </ScrollView>
  );
}

const styles = StyleSheet.create({
  container: {
    padding: 20,
  },
  titleContainer: {
    flexDirection: 'row',
    alignItems: 'center',
    marginBottom: 24,
    paddingTop: 12,
  },
  mainTitle: {
    fontSize: 28,
    fontWeight: '700',
    padding: 16,
   
   
    
  },
  cardsContainer: {
    gap: 20,
  },
  card: {
    borderRadius: 16,
    overflow: 'hidden',
    backgroundColor: '#FFFFFF',
    ...Platform.select({
      ios: {
        shadowColor: '#000',
        shadowOffset: { width: 0, height: 2 },
        shadowOpacity: 0.1,
        shadowRadius: 8,
      },
      android: {
        elevation: 4,
      },
    }),
  },
  cardHeader: {
    flexDirection: 'row',
    alignItems: 'center',
    paddingVertical: 16,
    paddingHorizontal: 20,
    backgroundColor: '#F8FAFC',
    borderBottomWidth: 1,
    borderBottomColor: '#E2E8F0',
  },
  cardIcon: {
    marginRight: 12,
  },
  cardTitle: {
    fontSize: 18,
    fontWeight: '600',
    color: '#1E293B',
  },
  cardContent: {
    padding: 20,
    gap: 12,
    backgroundColor: '#FFFFFF',
  },
  bulletItem: {
    flexDirection: 'row',
    alignItems: 'flex-start',
    paddingVertical: 4,
  },
  bulletPoint: {
    marginRight: 12,
    color: '#4F86F7',
    fontSize: 15,
    fontWeight: '600',
  },
  bulletText: {
    flex: 1,
    fontSize: 16,
    lineHeight: 24,
    color: '#334155',
  },
  footer: {
    marginTop: 40,
    marginBottom: 20,
    alignItems: 'center',
  },
  footerText: {
    marginTop: 16,
    fontSize: 13,
    color: '#64748B',
  },
});
