# ECG Firebase App Coding Guide

## Code Organization

### File Naming Conventions
- Use PascalCase for component files: `ECGChart.tsx`
- Use camelCase for utility files: `dataProcessor.ts`
- Use kebab-case for asset files: `ecg-waveform.svg`
- Use `.tsx` for React components, `.ts` for TypeScript files

### Directory Structure
```
src/
├── components/          # Reusable UI components
│   ├── common/         # Shared components
│   └── ecg/           # ECG-specific components
├── hooks/              # Custom React hooks
├── services/          # API and Firebase services
├── utils/             # Utility functions
└── types/             # TypeScript type definitions
```

## Component Development

### Basic Component Structure
```typescript
import React from 'react';
import { View, Text } from 'react-native';

interface Props {
  title: string;
  data: number[];
}

const ECGComponent: React.FC<Props> = ({ title, data }) => {
  return (
    <View>
      <Text>{title}</Text>
      {/* Component content */}
    </View>
  );
};

export default ECGComponent;
```

### Firebase Integration
```typescript
import { initializeApp } from 'firebase/app';
import { getFirestore } from 'firebase/firestore';

// Initialize Firebase
const firebaseConfig = {
  // Your Firebase config
};

const app = initializeApp(firebaseConfig);
const db = getFirestore(app);

// Example data fetching
const fetchECGData = async (userId: string) => {
  const docRef = doc(db, 'ecg_data', userId);
  const docSnap = await getDoc(docRef);
  return docSnap.data();
};
```

### Chart Implementation
```typescript
import React from 'react';
import Chart from 'react-apexcharts';

const ECGChart: React.FC = () => {
  const options = {
    chart: {
      type: 'line',
      animations: {
        enabled: true,
      },
    },
    xaxis: {
      type: 'datetime',
    },
  };

  const series = [{
    name: 'ECG',
    data: [], // Your ECG data points
  }];

  return (
    <Chart
      options={options}
      series={series}
      type="line"
      height={350}
    />
  );
};
```

## State Management

### Using Context
```typescript
import React, { createContext, useContext, useState } from 'react';

interface ECGContextType {
  ecgData: number[];
  setECGData: (data: number[]) => void;
}

const ECGContext = createContext<ECGContextType | undefined>(undefined);

export const ECGProvider: React.FC = ({ children }) => {
  const [ecgData, setECGData] = useState<number[]>([]);

  return (
    <ECGContext.Provider value={{ ecgData, setECGData }}>
      {children}
    </ECGContext.Provider>
  );
};

export const useECG = () => {
  const context = useContext(ECGContext);
  if (!context) {
    throw new Error('useECG must be used within an ECGProvider');
  }
  return context;
};
```

## Error Handling

### API Error Handling
```typescript
const fetchData = async () => {
  try {
    const response = await fetch('api/ecg');
    if (!response.ok) {
      throw new Error(`HTTP error! status: ${response.status}`);
    }
    const data = await response.json();
    return data;
  } catch (error) {
    console.error('Error fetching ECG data:', error);
    // Handle error appropriately
  }
};
```

## Testing

### Component Testing
```typescript
import { render, screen } from '@testing-library/react-native';
import ECGComponent from '../ECGComponent';

describe('ECGComponent', () => {
  it('renders correctly', () => {
    render(<ECGComponent title="Test ECG" data={[1, 2, 3]} />);
    expect(screen.getByText('Test ECG')).toBeTruthy();
  });
});
```

## Performance Optimization

### Memoization
```typescript
import React, { useMemo } from 'react';

const ECGChart: React.FC = ({ data }) => {
  const processedData = useMemo(() => {
    return data.map(point => ({
      x: point.timestamp,
      y: point.value
    }));
  }, [data]);

  return <Chart data={processedData} />;
};
```

## Best Practices

1. **Type Safety**
   - Always define interfaces for props
   - Use TypeScript's strict mode
   - Avoid using `any` type

2. **Component Design**
   - Keep components small and focused
   - Use composition over inheritance
   - Implement proper prop validation

3. **State Management**
   - Use local state for UI-specific state
   - Use context for global state
   - Implement proper error boundaries

4. **Performance**
   - Implement proper memoization
   - Use virtualized lists for long lists
   - Optimize re-renders

5. **Code Style**
   - Follow ESLint rules
   - Use Prettier for formatting
   - Write meaningful comments

## Common Patterns

### Data Fetching Pattern
```typescript
const useECGData = (userId: string) => {
  const [data, setData] = useState<ECGData[]>([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<Error | null>(null);

  useEffect(() => {
    const fetchData = async () => {
      try {
        const response = await fetchECGData(userId);
        setData(response);
      } catch (err) {
        setError(err as Error);
      } finally {
        setLoading(false);
      }
    };

    fetchData();
  }, [userId]);

  return { data, loading, error };
};
```

### Form Handling
```typescript
const ECGForm: React.FC = () => {
  const [formData, setFormData] = useState({
    patientId: '',
    date: '',
    notes: ''
  });

  const handleSubmit = async (e: React.FormEvent) => {
    e.preventDefault();
    try {
      await saveECGData(formData);
      // Handle success
    } catch (error) {
      // Handle error
    }
  };

  return (
    <form onSubmit={handleSubmit}>
      {/* Form fields */}
    </form>
  );
};
```

## Debugging Tips

1. **React Native Debugger**
   - Use React Native Debugger for state inspection
   - Implement proper logging
   - Use React DevTools

2. **Performance Monitoring**
   - Use React Native Performance Monitor
   - Implement proper error tracking
   - Monitor memory usage

3. **Common Issues**
   - Handle network errors gracefully
   - Implement proper loading states
   - Handle offline scenarios 