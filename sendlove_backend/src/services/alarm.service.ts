import { FirebaseAlarmRepository } from '../repositories/firebase/firebase-alarm.repository';
import { FirebaseBoxRepository } from '../repositories/firebase/firebase-box.repository';
import { Alarm } from '../types/alarm.types';
import { AppError } from '../middleware/error-handler.middleware';

export class AlarmService {
  private alarmRepo: FirebaseAlarmRepository;
  private boxRepo: FirebaseBoxRepository;

  constructor() {
    this.alarmRepo = new FirebaseAlarmRepository();
    this.boxRepo = new FirebaseBoxRepository();
  }

  private async notifyESP32(boxId: string) {
    await this.boxRepo.updatePollingCache(boxId, { hasNewAlarms: true });
  }

  async createAlarm(uid: string, boxId: string, data: Partial<Alarm>): Promise<Alarm> {
    // Note: We should ideally verify if uid is the receiver here, but that is typically done in the controller/middleware
    const alarms = await this.alarmRepo.listAlarms(boxId);
    if (alarms.length >= 10) {
      throw new AppError(400, 'alarm_limit_reached', 'Maximum 10 alarms allowed');
    }

    const alarmId = `alarm_${Date.now()}`;
    const newAlarm = await this.alarmRepo.createAlarm(boxId, alarmId, {
      ...data,
      createdAt: Date.now(),
    });

    await this.notifyESP32(boxId);
    return newAlarm;
  }

  async getAlarms(boxId: string): Promise<Alarm[]> {
    return this.alarmRepo.listAlarms(boxId);
  }

  async updateAlarm(boxId: string, alarmId: string, data: Partial<Alarm>): Promise<Alarm> {
    const alarm = await this.alarmRepo.getAlarm(boxId, alarmId);
    if (!alarm) throw new AppError(404, 'alarm_not_found', 'Alarm not found');

    const updated = await this.alarmRepo.updateAlarm(boxId, alarmId, data);
    await this.notifyESP32(boxId);
    return updated;
  }

  async deleteAlarm(boxId: string, alarmId: string): Promise<void> {
    const alarm = await this.alarmRepo.getAlarm(boxId, alarmId);
    if (!alarm) throw new AppError(404, 'alarm_not_found', 'Alarm not found');

    await this.alarmRepo.deleteAlarm(boxId, alarmId);
    await this.notifyESP32(boxId);
  }
}
