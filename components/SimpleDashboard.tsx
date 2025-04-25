import React, { useEffect, useState } from 'react';
import { View, Text, StyleSheet, Dimensions, ScrollView, SafeAreaView, StatusBar, Platform, Image } from 'react-native';
import { subscribeToLatestEcgData, EcgReading } from '../constants/EcgData';
import { FontAwesome } from '@expo/vector-icons';
import YakapLogo from '../assets/images/yakap_logo.svg';

const SimpleDashboard: React.FC = () => {
  const [latestReading, setLatestReading] = useState<EcgReading | null>(null);
  const [bpmHistory, setBpmHistory] = useState<number[]>([]);
  const [ecgHistory, setEcgHistory] = useState<number[]>([]);
  const [timeLabels, setTimeLabels] = useState<string[]>([]);
  const [isConnected, setIsConnected] = useState(false);
  const [lastUpdateTime, setLastUpdateTime] = useState<number>(Date.now());

  // Time formatting function
  const formatTime = (date: Date): string => {
    return `${date.getHours().toString().padStart(2, '0')}:${date.getMinutes().toString().padStart(2, '0')}:${date.getSeconds().toString().padStart(2, '0')}`;
  };

  // Monitor data updates and connection status
  useEffect(() => {
    let connectionCheckInterval: NodeJS.Timeout;

    const checkConnection = () => {
      const now = Date.now();
      const timeSinceLastUpdate = now - lastUpdateTime;
      if (timeSinceLastUpdate > 5000) { //  seconds threshold
        setIsConnected(false);
      }
    };

    // Start checking connection status
    connectionCheckInterval = setInterval(checkConnection, 1000); // Check every second

    return () => {
      clearInterval(connectionCheckInterval);
    };
  }, [lastUpdateTime]);

  // Subscribe to ECG data
  useEffect(() => {
    const unsubscribe = subscribeToLatestEcgData((data) => {
      if (data) {
        setLatestReading(data);
        setLastUpdateTime(Date.now());
        setIsConnected(true);
        
        // Add timestamp for x-axis
        const currentTime = new Date();
        const timeString = formatTime(currentTime);
        
        // Update BPM history
        setBpmHistory(prev => {
          const newHistory = [...prev, data.bpm];
          return newHistory.slice(-10); // Keep last 10 readings
        });
        
        // Update ECG history
        setEcgHistory(prev => {
          const newHistory = [...prev, data.smoothedEcg || 0];
          return newHistory.slice(-10); // Keep last 10 readings
        });
        
        // Update time labels
        setTimeLabels(prev => {
          const newLabels = [...prev, timeString];
          return newLabels.slice(-10); // Keep last 10 labels
        });
      }
    });

    // If no data, create sample data for demo purposes
    if (bpmHistory.length === 0) {
      const demoData = [];
      const demoLabels = [];
      const demoEcg = [];
      
      const now = new Date();
      for (let i = 0; i < 10; i++) {
        const time = new Date(now.getTime() - (9 - i) * 1000);
        demoLabels.push(formatTime(time));
        demoData.push(Math.floor(Math.random() * 20) + 70); // Random BPM between 70-90
        demoEcg.push(Math.random() * 50 + 50); // Random ECG values
      }
      
      setBpmHistory(demoData);
      setTimeLabels(demoLabels);
      setEcgHistory(demoEcg);
    }

    return () => unsubscribe();
  }, []);

  // Create a simple line chart for BPM
  const renderBPMChart = () => {
    const screenWidth = Dimensions.get('window').width;
    const width = screenWidth - 48; // Increased padding for better mobile display
    const height = 200;
    const maxBpm = Math.max(...(bpmHistory.length > 0 ? bpmHistory : [100]), 100);
    const minBpm = Math.min(...(bpmHistory.length > 0 ? bpmHistory : [60]), 60);
    const range = maxBpm - minBpm;
    
    const data = bpmHistory;
    
    return (
      <View style={styles.chartContainer}>
        <View style={styles.chartBg}>
          {/* Y-axis labels */}
          <View style={styles.yAxisLabels}>
            <Text style={styles.axisLabel}>{maxBpm}</Text>
            <Text style={styles.axisLabel}>{Math.round((maxBpm + minBpm) / 2)}</Text>
            <Text style={styles.axisLabel}>{minBpm}</Text>
          </View>
          
          {/* Chart grid lines */}
          <View style={styles.gridLine} />
          <View style={[styles.gridLine, { top: height / 2 }]} />
          <View style={[styles.gridLine, { top: height - 20 }]} />
          
          {data.length === 0 ? (
            <View style={styles.noDataContainer}>
              <Text style={styles.noDataText}>Waiting for data...</Text>
            </View>
          ) : (
            <>
              {/* Draw the line */}
              {data.map((value, index) => {
                if (index === 0) return null;
                
                const prevValue = data[index - 1];
                const x1 = ((index - 1) / (data.length - 1)) * width;
                const y1 = height - ((prevValue - minBpm) / range) * (height - 40);
                const x2 = (index / (data.length - 1)) * width;
                const y2 = height - ((value - minBpm) / range) * (height - 40);
                
                return (
                  <View 
                    key={`line-${index}`}
                    style={{
                      position: 'absolute',
                      left: x1 + 30,
                      top: y1,
                      width: Math.sqrt(Math.pow(x2 - x1, 2) + Math.pow(y2 - y1, 2)),
                      height: 3,
                      backgroundColor: '#4CAF50',
                      transform: [{
                        rotate: `${Math.atan2(y2 - y1, x2 - x1) * (180 / Math.PI)}deg`
                      }],
                      transformOrigin: 'left top',
                    }}
                  />
                );
              })}
              
              {/* Draw data points */}
              {data.map((value, index) => {
                const x = (index / (data.length - 1)) * width;
                const y = height - ((value - minBpm) / range) * (height - 40);
                
                return (
                  <View
                    key={`point-${index}`}
                    style={{
                      position: 'absolute',
                      left: x + 27,
                      top: y - 3,
                      width: 6,
                      height: 6,
                      borderRadius: 3,
                      backgroundColor: '#4CAF50',
                    }}
                  />
                );
              })}
            </>
          )}
          
          {/* X-axis labels */}
          <View style={styles.xAxisLabels}>
            {timeLabels.length > 0 
              ? timeLabels.filter((_, i) => i % 2 === 0).map((label, index) => (
                  <Text key={`xlabel-${index}`} style={styles.xAxisLabel}>
                    {label}
                  </Text>
                ))
              : <Text style={styles.xAxisLabel}>Waiting for data...</Text>
            }
          </View>
        </View>
      </View>
    );
  };

  // Create a simple area chart for ECG
  const renderECGChart = () => {
    const screenWidth = Dimensions.get('window').width;
    const width = screenWidth - 48;
    const height = 200;
    const data = ecgHistory;
    const maxEcg = data.length > 0 ? Math.max(...data) : 100;
    const minEcg = data.length > 0 ? Math.min(...data) : 0;
    const range = maxEcg - minEcg;
    
    return (
      <View style={styles.chartContainer}>
        <View style={[styles.chartBg, styles.ecgChartContainer]}>
          {/* Y-axis labels */}
          <View style={styles.yAxisLabels}>
            <Text style={styles.axisLabel}>{maxEcg.toFixed(0)}</Text>
            <Text style={styles.axisLabel}>{((maxEcg + minEcg) / 2).toFixed(0)}</Text>
            <Text style={styles.axisLabel}>{minEcg.toFixed(0)}</Text>
          </View>
          
          {/* Chart grid lines */}
          <View style={styles.gridLine} />
          <View style={[styles.gridLine, { top: height / 2 }]} />
          <View style={[styles.gridLine, { top: height - 20 }]} />
          
          {data.length === 0 ? (
            <View style={styles.noDataContainer}>
              <Text style={styles.noDataText}>Waiting for data...</Text>
            </View>
          ) : (
            <View style={styles.ecgChartArea}>
              {/* Connect points with lines */}
              {data.map((value, index) => {
                if (index === 0) return null;
                
                const prevValue = data[index - 1];
                const x1 = ((index - 1) / (data.length - 1)) * width;
                const y1 = height - ((prevValue - minEcg) / range) * (height - 40);
                const x2 = (index / (data.length - 1)) * width;
                const y2 = height - ((value - minEcg) / range) * (height - 40);
                
                return (
                  <View 
                    key={`line-${index}`}
                    style={{
                      position: 'absolute',
                      left: x1 + 30,
                      top: y1,
                      width: Math.sqrt(Math.pow(x2 - x1, 2) + Math.pow(y2 - y1, 2)),
                      height: 3,
                      backgroundColor: '#1E88E5',
                      transform: [{
                        rotate: `${Math.atan2(y2 - y1, x2 - x1) * (180 / Math.PI)}deg`
                      }],
                      transformOrigin: 'left top',
                    }}
                  />
                );
              })}
              
              {/* Draw data points */}
              {data.map((value, index) => {
                const x = (index / (data.length - 1)) * width;
                const y = height - ((value - minEcg) / range) * (height - 40);
                
                return (
                  <View
                    key={`point-${index}`}
                    style={{
                      position: 'absolute',
                      left: x + 27,
                      top: y - 4,
                      width: 8,
                      height: 8,
                      borderRadius: 4,
                      backgroundColor: '#1E88E5',
                    }}
                  />
                );
              })}
            </View>
          )}
          
          {/* X-axis labels */}
          <View style={styles.xAxisLabels}>
            {timeLabels.length > 0 
              ? timeLabels.filter((_, i) => i % 2 === 0).map((label, index) => (
                  <Text key={`xlabel-${index}`} style={styles.xAxisLabel}>
                    {label}
                  </Text>
                ))
              : <Text style={styles.xAxisLabel}>Waiting for data...</Text>
            }
          </View>
        </View>
      </View>
    );
  };

  return (
    <SafeAreaView style={styles.safeArea}>
      <StatusBar barStyle="dark-content" backgroundColor="#f5f7fa" />
      <ScrollView style={styles.container} contentContainerStyle={styles.contentContainer}>
        <View style={styles.header}>
          <YakapLogo width={80} height={80} style={styles.logo} />
          <Text style={styles.dashboardTitle}>Dashboard</Text>
        </View>
        
        {/* BPM and Device Status Cards */}
        <View style={styles.cardsRow}>
          {/* BPM Card */}
          <View style={[styles.bpmCard, styles.card]}>
            <View style={styles.heartIconContainer}>
              <FontAwesome name="heartbeat" size={24} color="#fff" style={styles.heartIcon} />
            </View>
            <Text style={styles.bpmValue}>
              {latestReading ? latestReading.bpm : '--'}
            </Text>
            <Text style={styles.bpmLabel}>BPM</Text>
            <Text style={styles.sensorLabel}>Latest reading</Text>
          </View>
          
          {/* Status Card */}
          <View style={[styles.statusCard, styles.card]}>
            <View style={styles.statusContainer}>
              <View style={styles.statusIconContainer}>
                <FontAwesome name="signal" size={24} color="#fff" style={styles.statusIcon} />
              </View>
              <Text style={styles.statusTitle}>Status</Text>
              <Text style={styles.statusValue}>{isConnected ? 'Connected' : 'Waiting'}</Text>
              <View style={[styles.statusIndicator, isConnected ? styles.statusConnected : styles.statusDisconnected]} />
            </View>
          </View>
        </View>
        
        {/* BPM Line Chart */}
        <View style={styles.chartCard}>
          <View style={styles.chartHeader}>
            <Text style={styles.chartTitle}>Real-Time BPM</Text>
            <Text style={styles.currentBpm}>
              Current: {latestReading ? `${latestReading.bpm} BPM` : 'No data'}
            </Text>
          </View>
          {renderBPMChart()}
        </View>
        
        {/* ECG Area Chart */}
        <View style={styles.chartCard}>
          <View style={styles.chartHeader}>
            <Text style={styles.chartTitle}>ECG Waveform</Text>
            <Text style={styles.lastUpdated}>
              Last update: {timeLabels.length > 0 ? timeLabels[timeLabels.length - 1] : '--:--:--'}
            </Text>
          </View>
          {renderECGChart()}
        </View>
        
        {/* Device Info Card */}
        <View style={styles.deviceCard}>
          <Text style={styles.deviceTitle}>Device Information</Text>
          <View style={styles.deviceInfo}>
            <Text style={styles.deviceLabel}>Device ID:</Text>
            <Text style={styles.deviceValue}>{latestReading ? latestReading.deviceId : '--'}</Text>
          </View>
          <View style={styles.deviceInfo}>
            <Text style={styles.deviceLabel}>Connection:</Text>
            <Text style={styles.deviceValue}>{isConnected ? 'Active' : 'Inactive'}</Text>
          </View>
          <View style={styles.deviceInfo}>
            <Text style={styles.deviceLabel}>Signal Quality:</Text>
            <Text style={styles.deviceValue}>{isConnected ? 'Good' : '--'}</Text>
          </View>
        </View>
      </ScrollView>
    </SafeAreaView>
  );
};

const styles = StyleSheet.create({
  safeArea: {
    flex: 1,
    backgroundColor: '#f5f7fa',
    paddingTop: Platform.OS === 'android' ? StatusBar.currentHeight : 0,
  },
  container: {
    flex: 1,
    backgroundColor: '#f5f7fa',
  },
  contentContainer: {
    padding: 16,
    paddingBottom: 32,
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
    marginRight: 16,
    shadowColor: '#000',
    shadowOffset: {
      width: 0,
      height: 2,
    },
    shadowOpacity: 0.1,
    shadowRadius: 3,
    elevation: 2,
  },
  dashboardTitle: {
    fontSize: 26,
    fontWeight: '700',
    color: '#1a237e',
    letterSpacing: 0.8,
    textShadowColor: 'rgba(0, 0, 0, 0.1)',
    textShadowOffset: { width: 0, height: 1 },
    textShadowRadius: 2,
  },
  cardsRow: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    marginBottom: 16,
  },
  card: {
    borderRadius: 12,
    padding: 16,
    shadowColor: '#000',
    shadowOffset: {
      width: 0,
      height: 2,
    },
    shadowOpacity: 0.1,
    shadowRadius: 4,
    elevation: 3,
  },
  bpmCard: {
    flex: 1,
    backgroundColor: '#4D8FCF',
    marginRight: 8,
    alignItems: 'center',
  },
  heartIconContainer: {
    width: 48,
    height: 48,
    borderRadius: 24,
    backgroundColor: 'rgba(255, 255, 255, 0.2)',
    justifyContent: 'center',
    alignItems: 'center',
    marginBottom: 12,
  },
  heartIcon: {
    alignSelf: 'center',
  },
  bpmValue: {
    fontSize: 36,
    fontWeight: 'bold',
    color: '#fff',
  },
  bpmLabel: {
    fontSize: 16,
    color: 'rgba(255, 255, 255, 0.8)',
    marginTop: 4,
  },
  sensorLabel: {
    fontSize: 12,
    color: 'rgba(255, 255, 255, 0.6)',
    marginTop: 8,
  },
  statusCard: {
    flex: 1,
    backgroundColor: '#fff',
    marginLeft: 8,
  },
  statusContainer: {
    alignItems: 'center',
  },
  statusIconContainer: {
    width: 48,
    height: 48,
    borderRadius: 24,
    backgroundColor: '#90A4AE',
    justifyContent: 'center',
    alignItems: 'center',
    marginBottom: 12,
  },
  statusIcon: {
    alignSelf: 'center',
  },
  statusTitle: {
    fontSize: 16,
    fontWeight: '600',
    color: '#37474F',
    marginBottom: 8,
  },
  statusValue: {
    fontSize: 14,
    color: '#546E7A',
    marginBottom: 8,
  },
  statusIndicator: {
    width: 12,
    height: 12,
    borderRadius: 6,
  },
  statusConnected: {
    backgroundColor: '#4CAF50',
  },
  statusDisconnected: {
    backgroundColor: '#F44336',
  },
  chartCard: {
    backgroundColor: '#fff',
    borderRadius: 12,
    padding: 16,
    marginBottom: 16,
    shadowColor: '#000',
    shadowOffset: {
      width: 0,
      height: 2,
    },
    shadowOpacity: 0.1,
    shadowRadius: 4,
    elevation: 3,
  },
  chartHeader: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    marginBottom: 16,
  },
  chartTitle: {
    fontSize: 18,
    fontWeight: '600',
    color: '#263238',
  },
  currentBpm: {
    fontSize: 14,
    color: '#4CAF50',
    fontWeight: '500',
  },
  lastUpdated: {
    fontSize: 14,
    color: '#1E88E5',
    fontWeight: '500',
  },
  chartContainer: {
    marginTop: 8,
  },
  chartBg: {
    height: 200,
    backgroundColor: '#f9f9f9',
    borderRadius: 8,
    overflow: 'hidden',
    position: 'relative',
    paddingTop: 10,
    paddingBottom: 30,
  },
  ecgChartContainer: {
    backgroundColor: '#f9f9f9',
  },
  ecgChartArea: {
    position: 'relative',
    height: '100%',
    width: '100%',
  },
  yAxisLabels: {
    position: 'absolute',
    left: 0,
    top: 0,
    bottom: 30,
    width: 30,
    justifyContent: 'space-between',
    paddingVertical: 10,
  },
  axisLabel: {
    fontSize: 10,
    color: '#90A4AE',
    textAlign: 'center',
  },
  xAxisLabels: {
    position: 'absolute',
    left: 30,
    right: 0,
    bottom: 0,
    height: 30,
    flexDirection: 'row',
    justifyContent: 'space-between',
    paddingHorizontal: 10,
    alignItems: 'center',
  },
  xAxisLabel: {
    fontSize: 10,
    color: '#90A4AE',
    textAlign: 'center',
  },
  gridLine: {
    position: 'absolute',
    left: 30,
    right: 0,
    top: 10,
    height: 1,
    backgroundColor: 'rgba(144, 164, 174, 0.2)',
  },
  deviceCard: {
    backgroundColor: '#fff',
    borderRadius: 12,
    padding: 16,
    shadowColor: '#000',
    shadowOffset: {
      width: 0,
      height: 2,
    },
    shadowOpacity: 0.1,
    shadowRadius: 4,
    elevation: 3,
  },
  deviceTitle: {
    fontSize: 18,
    fontWeight: '600',
    color: '#263238',
    marginBottom: 16,
  },
  deviceInfo: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    paddingVertical: 8,
    borderBottomWidth: 1,
    borderBottomColor: 'rgba(0, 0, 0, 0.05)',
  },
  deviceLabel: {
    fontSize: 14,
    color: '#546E7A',
  },
  deviceValue: {
    fontSize: 14,
    color: '#263238',
    fontWeight: '500',
  },
  noDataContainer: {
    position: 'absolute',
    top: 0,
    left: 0,
    right: 0,
    bottom: 0,
    justifyContent: 'center',
    alignItems: 'center',
  },
  noDataText: {
    fontSize: 14,
    color: '#90A4AE',
    fontStyle: 'italic',
  },
});

export default SimpleDashboard; 