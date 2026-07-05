export type AlarmType = 'one-shot' | 'recurring';

export interface Alarm {
  alarmId: string;
  time: string; // Format: "HH:mm" in 24h format
  type: AlarmType;
  daysOfWeek?: number[]; // 0 = Sunday, 1 = Monday, etc. Used if type is 'recurring'
  enabled: boolean;
  soundId: string;
  createdAt: number;
}
