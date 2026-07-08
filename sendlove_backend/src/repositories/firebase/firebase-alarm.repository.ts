import { Alarm } from '../../types/alarm.types';
import { db } from '../../firebase';
import { IAlarmRepository } from '../interfaces/alarm.repository.interface';

/**
 * Alarm được lưu tại: boxes/{boxId}/config/alarm_list/{alarmId}
 * Khi thay đổi alarm list, cần set flags/a_flag = true để ESP32 biết.
 */
export class FirebaseAlarmRepository implements IAlarmRepository {
  private getBasePath(boxId: string): string {
    return `boxes/${boxId}/config/alarm_list`;
  }

  async createAlarm(boxId: string, alarmId: string, data: Omit<Alarm, 'id'>): Promise<Alarm> {
    const ref = db.ref(`${this.getBasePath(boxId)}/${alarmId}`);
    const record = { id: alarmId, ...data };
    await ref.set(record);

    // Set a_flag = true để ESP32 biết alarm list đã thay đổi
    await db.ref(`boxes/${boxId}/flags/a_flag`).set(true);

    return record as Alarm;
  }

  async getAlarm(boxId: string, alarmId: string): Promise<Alarm | null> {
    const snapshot = await db.ref(`${this.getBasePath(boxId)}/${alarmId}`).once('value');
    if (!snapshot.exists()) return null;
    return snapshot.val() as Alarm;
  }

  async updateAlarm(boxId: string, alarmId: string, data: Partial<Alarm>): Promise<Alarm> {
    const ref = db.ref(`${this.getBasePath(boxId)}/${alarmId}`);
    await ref.update(data);

    // Set a_flag = true
    await db.ref(`boxes/${boxId}/flags/a_flag`).set(true);

    return this.getAlarm(boxId, alarmId) as Promise<Alarm>;
  }

  async deleteAlarm(boxId: string, alarmId: string): Promise<void> {
    await db.ref(`${this.getBasePath(boxId)}/${alarmId}`).remove();

    // Set a_flag = true
    await db.ref(`boxes/${boxId}/flags/a_flag`).set(true);
  }

  async listAlarms(boxId: string): Promise<Alarm[]> {
    const snapshot = await db.ref(this.getBasePath(boxId)).once('value');
    if (!snapshot.exists()) return [];

    const alarmsObj = snapshot.val();
    const alarms: Alarm[] = [];

    for (const id in alarmsObj) {
      alarms.push(alarmsObj[id]);
    }

    return alarms;
  }
}
