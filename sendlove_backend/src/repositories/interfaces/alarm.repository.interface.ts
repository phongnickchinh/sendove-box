import { Alarm } from '../../types/alarm.types';

export interface IAlarmRepository {
  createAlarm(boxId: string, alarmId: string, data: Omit<Alarm, 'id'>): Promise<Alarm>;
  getAlarm(boxId: string, alarmId: string): Promise<Alarm | null>;
  updateAlarm(boxId: string, alarmId: string, data: Partial<Alarm>): Promise<Alarm>;
  deleteAlarm(boxId: string, alarmId: string): Promise<void>;
  listAlarms(boxId: string): Promise<Alarm[]>;
}
