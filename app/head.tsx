import { Stack } from 'expo-router';

export default function CustomHead() {
  return (
    <Stack.Screen
      options={{
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
      }}
    />
  );
} 