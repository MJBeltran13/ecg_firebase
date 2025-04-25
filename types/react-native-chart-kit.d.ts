declare module 'react-native-chart-kit' {
  import { ReactNode } from 'react';
  import { ViewStyle, TextStyle } from 'react-native';

  export interface ChartConfig {
    backgroundGradientFrom?: string;
    backgroundGradientTo?: string;
    backgroundGradientFromOpacity?: number;
    backgroundGradientToOpacity?: number;
    fillShadowGradientFrom?: string;
    fillShadowGradientTo?: string;
    color?: (opacity?: number) => string;
    labelColor?: (opacity?: number) => string;
    strokeWidth?: number;
    barPercentage?: number;
    useShadowColorFromDataset?: boolean;
    decimalPlaces?: number;
    style?: ViewStyle;
    propsForDots?: {
      r?: string;
      strokeWidth?: string;
      stroke?: string;
    };
    propsForBackgroundLines?: {
      strokeWidth?: number;
      stroke?: string;
      strokeDasharray?: string;
    };
    propsForLabels?: TextStyle;
  }

  export interface AbstractChartProps {
    width: number;
    height: number;
    chartConfig: ChartConfig;
    style?: ViewStyle;
    bezier?: boolean;
    fromZero?: boolean;
    withInnerLines?: boolean;
    withOuterLines?: boolean;
    withHorizontalLabels?: boolean;
    withVerticalLabels?: boolean;
    withDots?: boolean;
    withShadow?: boolean;
    withScrollableDot?: boolean;
    yAxisLabel?: string;
    yAxisSuffix?: string;
    yAxisInterval?: number;
    xAxisLabel?: string;
    xAxisSuffix?: string;
    backgroundColor?: string;
    segments?: number;
    formatYLabel?: (yLabel: string) => string;
    formatXLabel?: (xLabel: string) => string;
  }

  export interface LineChartData {
    labels: string[];
    datasets: {
      data: number[];
      color?: (opacity?: number) => string;
      strokeWidth?: number;
      withDots?: boolean;
      withScrollableDot?: boolean;
    }[];
    legend?: string[];
  }

  export interface AreaChartData {
    labels: string[];
    datasets: {
      data: number[];
      color?: (opacity?: number) => string;
      strokeWidth?: number;
      withDots?: boolean;
      withScrollableDot?: boolean;
    }[];
    legend?: string[];
  }

  export class LineChart extends React.Component<AbstractChartProps & { data: LineChartData }> {}
  export class AreaChart extends React.Component<AbstractChartProps & { data: AreaChartData }> {}
} 