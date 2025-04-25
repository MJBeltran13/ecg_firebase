import { initializeApp } from 'firebase/app';
import { getDatabase } from 'firebase/database';

// Your web app's Firebase configuration
// Replace these values with your actual Firebase config values from:
// Firebase Console -> Your Project -> Project Settings -> General -> Your Apps -> SDK setup and configuration
const firebaseConfig = {
  databaseURL: 'https://ecgdata-f042a-default-rtdb.asia-southeast1.firebasedatabase.app/',
  // You'll need to add these from your Firebase project settings
  apiKey: "AIzaSyA0OGrnWnNx0LDPGzDZHdrzajiRGEjr3AM",
  authDomain: "ecgdata-f042a.firebaseapp.com",
  projectId: "ecgdata-f042a",
  storageBucket: "ecgdata-f042a.appspot.com",
  messagingSenderId: "YOUR_MESSAGING_SENDER_ID",
  appId: "YOUR_APP_ID"
};

// Initialize Firebase
const app = initializeApp(firebaseConfig);
const database = getDatabase(app);

export { database }; 