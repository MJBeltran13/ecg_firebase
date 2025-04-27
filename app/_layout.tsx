import { DarkTheme, DefaultTheme, ThemeProvider } from '@react-navigation/native';
import { useFonts } from 'expo-font';
import { Stack } from 'expo-router';
import * as SplashScreen from 'expo-splash-screen';
import { StatusBar } from 'expo-status-bar';
import { useEffect } from 'react';
import { Platform } from 'react-native';
import 'react-native-reanimated';

import { useColorScheme } from '@/hooks/useColorScheme';

// Prevent the splash screen from auto-hiding before asset loading is complete.
SplashScreen.preventAutoHideAsync();

export default function RootLayout() {
  const colorScheme = useColorScheme();
  const [loaded] = useFonts({
    SpaceMono: require('../assets/fonts/SpaceMono-Regular.ttf'),
  });

  useEffect(() => {
    if (loaded) {
      SplashScreen.hideAsync();
    }
  }, [loaded]);

  if (!loaded) {
    return null;
  }

  return (
    <ThemeProvider value={colorScheme === 'dark' ? DarkTheme : DefaultTheme}>
      <Stack>
        <Stack.Screen 
          name="(tabs)" 
          options={{ 
            headerShown: false,
            ...(Platform.OS === 'web' && {
              title: 'ECG Monitor',
              meta: {
                viewport: 'width=device-width, initial-scale=1.0',
                'og:type': 'website',
                'og:title': 'ECG Monitor',
                'og:description': 'Real-time ECG monitoring application',
                'og:image': '/assets/images/logo2.jpg',
                'twitter:card': 'summary_large_image',
                'twitter:title': 'ECG Monitor',
                'twitter:description': 'Real-time ECG monitoring application',
                'twitter:image': '/assets/images/logo2.jpg',
              },
              links: [
                {
                  rel: 'icon',
                  type: 'image/jpeg',
                  href: '/assets/images/logo2.jpg',
                },
              ],
            }),
          }} 
        />
        <Stack.Screen name="+not-found" />
      </Stack>
      <StatusBar style="auto" />
    </ThemeProvider>
  );
}
