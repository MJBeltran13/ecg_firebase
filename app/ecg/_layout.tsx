import { Stack } from 'expo-router';

export default function EcgLayout() {
  return (
    <Stack>
      <Stack.Screen
        name="saved-sessions"
        options={{
          title: 'Saved Sessions',
          headerShown: true,
        }}
      />
      <Stack.Screen
        name="session-details"
        options={{
          title: 'Session Details',
          headerShown: true,
        }}
      />
    </Stack>
  );
} 