# Welcome to your Expo app 👋

This is an [Expo](https://expo.dev) project created with [`create-expo-app`](https://www.npmjs.com/package/create-expo-app).

# ECG Firebase Integration

This project integrates an ECG monitoring device with Firebase Realtime Database. It includes both an Arduino-based ESP32 ECG device and a React Native Expo app that displays the data.

## Get started

1. Configure Firebase
   
   Update the Firebase configuration in `constants/Firebase.ts` with your project credentials from the Firebase console.

2. Install dependencies

   ```bash
   npm install
   ```

3. Start the app

   ```bash
   npx expo start
   ```

## Arduino Setup

The Arduino code in `arduino/ecg.ino` sends ECG data to Firebase. To set it up:

1. Update the Wi-Fi credentials:
   ```c
   const char *ssid = "YOUR_WIFI_SSID";
   const char *password = "YOUR_WIFI_PASSWORD";
   ```

2. Upload the code to your ESP32 device with the AD8232 ECG sensor connected.

3. The device will send readings to Firebase when it detects valid heartbeats.

## Testing with Postman

You can test the Firebase integration using Postman without the physical device:

1. See the guide in `docs/firebase-postman-guide.md` for detailed instructions.

2. Use Postman to send sample ECG readings to your Firebase database.

## Firebase Database Structure

The database stores a single ECG reading with this structure:

```json
{
  "deviceId": "esp32",
  "bpm": 75,
  "timestamp": "2023-06-01T12:30:45",
  "rawEcg": 512,
  "smoothedEcg": 30
}
```

The data is stored at `ecg_readings` in the Firebase Realtime Database:
```
ecg_readings/
  ├── deviceId: "esp32"
  ├── bpm: 75
  ├── timestamp: "2023-06-01T12:30:45"
  ├── rawEcg: 512
  └── smoothedEcg: 30
```

This approach stores only the most recent reading, which is suitable for real-time monitoring.

## Learn more

To learn more about developing with Expo and Firebase:

- [Expo documentation](https://docs.expo.dev/)
- [Firebase documentation](https://firebase.google.com/docs)
- [Firebase Realtime Database REST API](https://firebase.google.com/docs/database/rest/start)

## Join the community

Join our community of developers creating universal apps.

- [Expo on GitHub](https://github.com/expo/expo): View our open source platform and contribute.
- [Discord community](https://chat.expo.dev): Chat with Expo users and ask questions.
