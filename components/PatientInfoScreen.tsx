import React, { useState, useEffect } from 'react';
import {
  View,
  Text,
  StyleSheet,
  TextInput,
  TouchableOpacity,
  Platform,
  KeyboardAvoidingView,
  ScrollView,
  Image,
  Alert,
  ActivityIndicator
} from 'react-native';
import { FontAwesome } from '@expo/vector-icons';
import DateTimePicker from '@react-native-community/datetimepicker';
import { savePatientInfo, PatientInfo, startEcgSession } from '../constants/LocalStorage';
import { useRouter } from 'expo-router';

interface PatientInfoScreenProps {
  onStartRecording: () => void;
}

const PatientInfoScreen: React.FC<PatientInfoScreenProps> = ({ onStartRecording }) => {
  const router = useRouter();
  const [name, setName] = useState('');
  const [monthsPregnant, setMonthsPregnant] = useState('');
  const [date, setDate] = useState(new Date());
  const [time, setTime] = useState(new Date());
  const [showDatePicker, setShowDatePicker] = useState(false);
  const [showTimePicker, setShowTimePicker] = useState(false);
  const [isLoading, setIsLoading] = useState(false);
  const [validationError, setValidationError] = useState('');

  // Format date for display
  const formatDate = (date: Date): string => {
    return date.toLocaleDateString('en-US', {
      year: 'numeric',
      month: 'short',
      day: 'numeric'
    });
  };

  // Format time for display
  const formatTime = (time: Date): string => {
    return time.toLocaleTimeString('en-US', {
      hour: '2-digit',
      minute: '2-digit'
    });
  };

  // Handle date change
  const onDateChange = (event: any, selectedDate?: Date) => {
    setShowDatePicker(Platform.OS === 'ios');
    if (selectedDate) {
      setDate(selectedDate);
    }
  };

  // Handle time change
  const onTimeChange = (event: any, selectedTime?: Date) => {
    setShowTimePicker(Platform.OS === 'ios');
    if (selectedTime) {
      setTime(selectedTime);
    }
  };

  // Validate inputs
  const validateInputs = (): boolean => {
    if (!name.trim()) {
      setValidationError('Patient name is required');
      return false;
    }

    if (!monthsPregnant.trim()) {
      setValidationError('Months pregnant is required');
      return false;
    }

    const monthsValue = parseInt(monthsPregnant, 10);
    if (isNaN(monthsValue) || monthsValue < 1 || monthsValue > 10) {
      setValidationError('Months pregnant must be between 1 and 10');
      return false;
    }

    setValidationError('');
    return true;
  };

  // Handle start recording button press
  const handleStartRecording = async () => {
    if (!validateInputs()) {
      return;
    }

    setIsLoading(true);
    try {
      const patientInfo: PatientInfo = {
        name: name.trim(),
        monthsPregnant: parseInt(monthsPregnant, 10),
        recordingDate: formatDate(date),
        recordingTime: formatTime(time)
      };

      await savePatientInfo(patientInfo);
      await startEcgSession(patientInfo);
      
      // Call the onStartRecording callback to navigate to the dashboard
      onStartRecording();
    } catch (error) {
      console.error('Error starting recording:', error);
      Alert.alert('Error', 'Failed to start recording. Please try again.');
    } finally {
      setIsLoading(false);
    }
  };

  return (
    <KeyboardAvoidingView 
      style={styles.container} 
      behavior={Platform.OS === 'ios' ? 'padding' : undefined}
    >
      <ScrollView contentContainerStyle={styles.scrollContainer}>
        <View style={styles.header}>
          <Image 
            source={require('../assets/images/logo2.jpg')} 
            style={styles.logo} 
            resizeMode="contain"
          />
          <Text style={styles.title}>Yakap App</Text>
        </View>

        <View style={styles.formContainer}>
          <Text style={styles.formTitle}>Patient Information</Text>
          
          {validationError ? (
            <View style={styles.errorContainer}>
              <FontAwesome name="exclamation-circle" size={16} color="#F44336" />
              <Text style={styles.errorText}>{validationError}</Text>
            </View>
          ) : null}

          <View style={styles.inputGroup}>
            <Text style={styles.label}>Patient Name</Text>
            <TextInput
              style={styles.input}
              value={name}
              onChangeText={setName}
              placeholder="Enter patient's full name"
              placeholderTextColor="#90A4AE"
            />
          </View>

          <View style={styles.inputGroup}>
            <Text style={styles.label}>Months Pregnant</Text>
            <TextInput
              style={styles.input}
              value={monthsPregnant}
              onChangeText={setMonthsPregnant}
              placeholder="Enter months pregnant (1-9)"
              keyboardType="number-pad"
              maxLength={2}
              placeholderTextColor="#90A4AE"
            />
          </View>

          <View style={styles.inputGroup}>
            <Text style={styles.label}>Recording Date</Text>
            <TouchableOpacity 
              style={styles.dateTimeButton}
              onPress={() => setShowDatePicker(true)}
            >
              <Text style={styles.dateTimeText}>{formatDate(date)}</Text>
              <FontAwesome name="calendar" size={18} color="#4D8FCF" />
            </TouchableOpacity>
            {showDatePicker && (
              <DateTimePicker
                value={date}
                mode="date"
                display="default"
                onChange={onDateChange}
                maximumDate={new Date()}
              />
            )}
          </View>

          <View style={styles.inputGroup}>
            <Text style={styles.label}>Recording Time</Text>
            <TouchableOpacity 
              style={styles.dateTimeButton}
              onPress={() => setShowTimePicker(true)}
            >
              <Text style={styles.dateTimeText}>{formatTime(time)}</Text>
              <FontAwesome name="clock-o" size={18} color="#4D8FCF" />
            </TouchableOpacity>
            {showTimePicker && (
              <DateTimePicker
                value={time}
                mode="time"
                display="default"
                onChange={onTimeChange}
              />
            )}
          </View>

          <TouchableOpacity 
            style={styles.startButton}
            onPress={handleStartRecording}
            disabled={isLoading}
          >
            {isLoading ? (
              <ActivityIndicator color="#FFFFFF" size="small" />
            ) : (
              <>
                <FontAwesome name="play-circle" size={20} color="#FFFFFF" />
                <Text style={styles.startButtonText}>Start Recording</Text>
              </>
            )}
          </TouchableOpacity>
        </View>
      </ScrollView>
    </KeyboardAvoidingView>
  );
};

const styles = StyleSheet.create({
  container: {
    flex: 1,
    backgroundColor: '#f5f7fa',
  },
  scrollContainer: {
    flexGrow: 1,
    padding: 16,
  },
  header: {
    flexDirection: 'row',
    alignItems: 'center',
    marginBottom: 24,
    backgroundColor: '#ffffff',
    padding: 16,
    borderRadius: 12,
    shadowColor: '#000',
    shadowOffset: {
      width: 0,
      height: 2,
    },
    shadowOpacity: 0.1,
    shadowRadius: 4,
    elevation: 3,
  },
  logo: {
    width: 80,
    height: 80,
    marginRight: 16,
  },
  title: {
    fontSize: 26,
    fontWeight: '700',
    color: '#4D8FCF',
    letterSpacing: 0.8,
  },
  formContainer: {
    backgroundColor: '#FFFFFF',
    borderRadius: 12,
    padding: 20,
    shadowColor: '#000',
    shadowOffset: {
      width: 0,
      height: 2,
    },
    shadowOpacity: 0.1,
    shadowRadius: 4,
    elevation: 3,
  },
  formTitle: {
    fontSize: 20,
    fontWeight: '600',
    color: '#263238',
    marginBottom: 20,
    textAlign: 'center',
  },
  inputGroup: {
    marginBottom: 20,
  },
  label: {
    fontSize: 16,
    fontWeight: '500',
    color: '#546E7A',
    marginBottom: 8,
  },
  input: {
    backgroundColor: '#F5F7FA',
    borderRadius: 8,
    padding: 12,
    fontSize: 16,
    color: '#263238',
    borderWidth: 1,
    borderColor: '#E0E0E0',
  },
  dateTimeButton: {
    backgroundColor: '#F5F7FA',
    borderRadius: 8,
    padding: 12,
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    borderWidth: 1,
    borderColor: '#E0E0E0',
  },
  dateTimeText: {
    fontSize: 16,
    color: '#263238',
  },
  startButton: {
    backgroundColor: '#4D8FCF',
    borderRadius: 8,
    padding: 16,
    flexDirection: 'row',
    justifyContent: 'center',
    alignItems: 'center',
    marginTop: 10,
    shadowColor: '#000',
    shadowOffset: {
      width: 0,
      height: 2,
    },
    shadowOpacity: 0.1,
    shadowRadius: 3,
    elevation: 2,
  },
  startButtonText: {
    color: '#FFFFFF',
    fontSize: 18,
    fontWeight: '600',
    marginLeft: 8,
  },
  errorContainer: {
    flexDirection: 'row',
    alignItems: 'center',
    backgroundColor: '#FFEBEE',
    padding: 10,
    borderRadius: 8,
    marginBottom: 16,
  },
  errorText: {
    color: '#F44336',
    marginLeft: 8,
    fontSize: 14,
  },
});

export default PatientInfoScreen; 