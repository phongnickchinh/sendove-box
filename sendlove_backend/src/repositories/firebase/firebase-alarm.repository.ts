import { Alarm } from '../../types/alarm.types';
import { db } from '../../firebase';

export class FirebaseAlarmRepository {
  // Alarms are stored under `alarms/{boxId}/{alarmId}`

  async createAlarm(boxId: string, alarmId: string, data: Partial<Alarm>): Promise<Alarm> {
    const ref = db.ref(`alarms/${boxId}/${alarmId}`);
    await ref.set(data);
    return this.getAlarm(boxId, alarmId) as Promise<Alarm>;
  }

  async getAlarm(boxId: string, alarmId: string): Promise<Alarm | null> {
    const snapshot = await db.ref(`alarms/${boxId}/${alarmId}`).once('value');
    if (!snapshot.exists()) return null;
    return { alarmId, ...snapshot.val() } as Alarm;
  }

  async updateAlarm(boxId: string, alarmId: string, data: Partial<Alarm>): Promise<Alarm> {
    const ref = db.ref(`alarms/${boxId}/${alarmId}`);
    await ref.update(data);
    return this.getAlarm(boxId, alarmId) as Promise<Alarm>;
  }

  async deleteAlarm(boxId: string, alarmId: string): Promise<void> {
    const ref = db.ref(`alarms/${boxId}/${alarmId}`);
    await ref.remove();
  }

  async listAlarms(boxId: string): Promise<Alarm[]> {
    const snapshot = await db.ref(`alarms/${boxId}`).once('value');
    if (!snapshot.exists()) return [];

    const alarmsObj = snapshot.val();
    const alarms: Alarm[] = [];

    for (const id in alarmsObj) {
      alarms.push({ alarmId: id, ...alarmsObj[id] });
    }

    return alarms;
  }
}
