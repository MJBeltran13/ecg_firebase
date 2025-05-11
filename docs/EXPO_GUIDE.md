# Expo Development Guide

## Setting Up Expo Go

### Prerequisites
- Node.js (v14 or newer)
- npm or yarn
- A smartphone with Expo Go app installed (for testing)

### Initialize a New Expo Project
1. Install Expo CLI globally:
```bash
npm install -g expo-cli
```

2. Create a new Expo project:
```bash
npx create-expo-app MyExpoApp
```

3. Navigate to your project directory:
```bash
cd MyExpoApp
```

## Starting the Development Server

1. Launch the development server:
```bash
npx expo start
```

2. You'll see a QR code in your terminal and a development server will open in your browser.

3. Options for running your app:
   - Scan the QR code with Expo Go app on Android or iOS Camera app on iPhone
   - Press `a` to open on Android emulator
   - Press `i` to open on iOS simulator
   - Press `w` to open in a web browser

## Creating an APK for Android

### Method 1: Using EAS Build (Recommended)

1. Install EAS CLI:
```bash
npm install -g eas-cli
```

2. Log in to your Expo account:
```bash
eas login
```

3. Configure EAS Build:
```bash
eas build:configure
```

4. Create a build profile in `eas.json`:
```json
{
  "build": {
    "preview": {
      "android": {
        "buildType": "apk"
      }
    }
  }
}
```

5. Start the build process:
```bash
eas build -p android --profile preview
```

6. Follow the prompts and wait for the build to complete (this will happen on Expo's servers)

7. Download your APK when the build is finished

### Method 2: Using Expo Build (Legacy)

1. Start the build process:
```bash
expo build:android -t apk
```

2. Follow the prompts and wait for the build to complete

3. Download your APK when the build is finished

## Tips for Development

- Edit `App.js` to modify your application
- Install new dependencies: `npm install package-name`
- Add new screens in separate files in a `/screens` folder
- Use Expo's documentation for access to device features (camera, location, etc.)
- Check Expo status at [status.expo.dev](https://status.expo.dev) if you encounter server issues

## Troubleshooting

- If the development server fails to start, try clearing the cache: `npx expo start -c`
- For connection issues, ensure your phone and computer are on the same network
- Check Expo's documentation at [docs.expo.dev](https://docs.expo.dev) for detailed information 