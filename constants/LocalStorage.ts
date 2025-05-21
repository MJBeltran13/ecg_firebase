import AsyncStorage from '@react-native-async-storage/async-storage';

// Patient information interface
export interface PatientInfo {
  name: string;
  monthsPregnant: number;
  recordingDate: string;
  recordingTime: string;
}

// ECG recording session interface
export interface EcgSession {
  patientInfo: PatientInfo;
  readings: EcgReading[];
  startTime: string;
  endTime?: string;
}

// Interface for ECG reading data (similar to the one in EcgData.ts)
export interface EcgReading {
  deviceId: string;
  bpm: number;
  timestamp: string;
  rawEcg?: number;
  smoothedEcg?: number;
}

// Save patient info before starting a session
export const savePatientInfo = async (patientInfo: PatientInfo): Promise<void> => {
  try {
    await AsyncStorage.setItem('current_patient_info', JSON.stringify(patientInfo));
  } catch (error) {
    console.error('Error saving patient info:', error);
    throw error;
  }
};

// Get current patient info
export const getCurrentPatientInfo = async (): Promise<PatientInfo | null> => {
  try {
    const patientInfoString = await AsyncStorage.getItem('current_patient_info');
    return patientInfoString ? JSON.parse(patientInfoString) : null;
  } catch (error) {
    console.error('Error getting patient info:', error);
    return null;
  }
};

// Start a new ECG recording session
export const startEcgSession = async (patientInfo: PatientInfo): Promise<string> => {
  try {
    const sessionId = `session_${Date.now()}`;
    const session: EcgSession = {
      patientInfo,
      readings: [],
      startTime: new Date().toISOString(),
    };
    
    await AsyncStorage.setItem(sessionId, JSON.stringify(session));
    await AsyncStorage.setItem('current_session_id', sessionId);
    
    return sessionId;
  } catch (error) {
    console.error('Error starting ECG session:', error);
    throw error;
  }
};

// Add ECG reading to current session
export const addEcgReading = async (reading: EcgReading): Promise<void> => {
  try {
    const sessionId = await AsyncStorage.getItem('current_session_id');
    if (!sessionId) {
      throw new Error('No active ECG session found');
    }
    
    const sessionString = await AsyncStorage.getItem(sessionId);
    if (!sessionString) {
      throw new Error('Session data not found');
    }
    
    const session: EcgSession = JSON.parse(sessionString);
    session.readings.push(reading);
    
    await AsyncStorage.setItem(sessionId, JSON.stringify(session));
  } catch (error) {
    console.error('Error adding ECG reading:', error);
    throw error;
  }
};

// End current ECG session
export const endEcgSession = async (): Promise<void> => {
  try {
    const sessionId = await AsyncStorage.getItem('current_session_id');
    if (!sessionId) {
      return;
    }
    
    const sessionString = await AsyncStorage.getItem(sessionId);
    if (!sessionString) {
      return;
    }
    
    const session: EcgSession = JSON.parse(sessionString);
    session.endTime = new Date().toISOString();
    
    await AsyncStorage.setItem(sessionId, JSON.stringify(session));
    await AsyncStorage.removeItem('current_session_id');
  } catch (error) {
    console.error('Error ending ECG session:', error);
    throw error;
  }
};

// Get all saved ECG sessions
export const getAllEcgSessions = async (): Promise<EcgSession[]> => {
  try {
    const keys = await AsyncStorage.getAllKeys();
    const sessionKeys = keys.filter(key => key.startsWith('session_'));
    
    const sessions: EcgSession[] = [];
    for (const key of sessionKeys) {
      const sessionString = await AsyncStorage.getItem(key);
      if (sessionString) {
        sessions.push(JSON.parse(sessionString));
      }
    }
    
    return sessions.sort((a, b) => 
      new Date(b.startTime).getTime() - new Date(a.startTime).getTime()
    );
  } catch (error) {
    console.error('Error getting all ECG sessions:', error);
    return [];
  }
};

// Clear all stored ECG sessions
export const clearAllEcgSessions = async (): Promise<void> => {
  try {
    const keys = await AsyncStorage.getAllKeys();
    const sessionKeys = keys.filter(key => key.startsWith('session_'));
    
    if (sessionKeys.length > 0) {
      await AsyncStorage.multiRemove(sessionKeys);
    }
  } catch (error) {
    console.error('Error clearing ECG sessions:', error);
    throw error;
  }
};

// Delete a specific ECG session
export const deleteEcgSession = async (sessionStartTime: string): Promise<void> => {
  try {
    console.log('Starting deletion process for session:', sessionStartTime);
    // First get all sessions to find the correct session ID
    const keys = await AsyncStorage.getAllKeys();
    console.log('All keys:', keys);
    const sessionKeys = keys.filter(key => key.startsWith('session_'));
    console.log('Session keys:', sessionKeys);
    
    // Find the session with matching startTime
    for (const key of sessionKeys) {
      const sessionString = await AsyncStorage.getItem(key);
      if (sessionString) {
        const session = JSON.parse(sessionString);
        console.log('Checking session:', key, 'startTime:', session.startTime);
        if (session.startTime === sessionStartTime) {
          console.log('Found matching session, deleting key:', key);
          await AsyncStorage.removeItem(key);
          console.log('Session deleted successfully');
          return;
        }
      }
    }
    console.log('No matching session found');
    throw new Error('Session not found');
  } catch (error) {
    console.error('Error in deleteEcgSession:', error);
    throw error;
  }
}; 