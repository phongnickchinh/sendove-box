import { FirebaseAlarmRepository } from '../repositories/firebase/firebase-alarm.repository';
import { Alarm } from '../types/alarm.types';
import { AppError } from '../middleware/error-handler.middleware';

export class AlarmService {
  private alarmRepo: FirebaseAlarmRepository;

  constructor() {
    this.alarmRepo = new FirebaseAlarmRepository();
  }

  /**
   * Tạo alarm mới (max 10 alarms / box).
   * Repository tự set a_flag = true khi tạo.
   */
  async createAlarm(boxId: string, data: { time: string; is_enable: boolean; repeatable: boolean }): Promise<Alarm> {
    const alarms = await this.alarmRepo.listAlarms(boxId);
    if (alarms.length >= 10) {
      throw new AppError(400, 'alarm_limit_reached', 'Maximum 10 alarms allowed');
    }

    const now = Date.now();
    const alarmId = `alarm_${now}`;

    return this.alarmRepo.createAlarm(boxId, alarmId, {
      time: data.time,
      is_enable: data.is_enable,
      repeatable: data.repeatable,
      created_at: now,
      updated_at: now,
    });
  }

  async getAlarms(boxId: string): Promise<Alarm[]> {
    return this.alarmRepo.listAlarms(boxId);
  }

  async updateAlarm(boxId: string, alarmId: string, data: Partial<Alarm>): Promise<Alarm> {
    const alarm = await this.alarmRepo.getAlarm(boxId, alarmId);
    if (!alarm) throw new AppError(404, 'alarm_not_found', 'Alarm not found');

    return this.alarmRepo.updateAlarm(boxId, alarmId, {
      ...data,
      updated_at: Date.now(),
    });
  }

  async deleteAlarm(boxId: string, alarmId: string): Promise<void> {
    const alarm = await this.alarmRepo.getAlarm(boxId, alarmId);
    if (!alarm) throw new AppError(404, 'alarm_not_found', 'Alarm not found');

    await this.alarmRepo.deleteAlarm(boxId, alarmId);
  }
}
