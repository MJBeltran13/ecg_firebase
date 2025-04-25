import { ref, set, onValue, off } from 'firebase/database';
import { database } from './Firebase';

// Interface for ECG reading data
export interface EcgReading {
  deviceId: string;
  bpm: number;
  timestamp: string;
  rawEcg?: number;
  smoothedEcg?: number;
}

// Write ECG data to the database
export const writeEcgData = (data: EcgReading) => {
  const ecgRef = ref(database, 'ecg_readings');
  return set(ecgRef, data);
};

// Subscribe to the latest ECG reading
export const subscribeToLatestEcgData = (callback: (data: EcgReading | null) => void) => {
  const ecgRef = ref(database, 'ecg_readings');
  
  onValue(ecgRef, (snapshot) => {
    if (snapshot.exists()) {
      const data = snapshot.val();
      callback(data);
    } else {
      callback(null);
    }
  });
  
  // Return a function to unsubscribe
  return () => off(ecgRef);
}; 