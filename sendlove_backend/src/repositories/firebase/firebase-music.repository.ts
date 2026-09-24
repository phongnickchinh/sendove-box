import { db } from '../../firebase';
import { AlarmMusic } from '../../types/music.types';
import { IMusicRepository } from '../interfaces/music.repository.interface';

/** boxes/{boxId}/music/{musicId} — xem music.types.ts */
export class FirebaseMusicRepository implements IMusicRepository {
  private base = (boxId: string) => `boxes/${boxId}/music`;

  async list(boxId: string): Promise<AlarmMusic[]> {
    const snap = await db.ref(this.base(boxId)).once('value');
    const val = (snap.val() || {}) as Record<string, AlarmMusic>;
    return Object.values(val).sort((a, b) => (a.created_at || 0) - (b.created_at || 0));
  }

  async get(boxId: string, musicId: string): Promise<AlarmMusic | null> {
    const snap = await db.ref(`${this.base(boxId)}/${musicId}`).once('value');
    return snap.exists() ? (snap.val() as AlarmMusic) : null;
  }

  async save(boxId: string, music: AlarmMusic): Promise<void> {
    await db.ref().update({
      [`${this.base(boxId)}/${music.music_id}`]: music,
      [`boxes/${boxId}/flags/music_flag`]: true,
    });
  }

  async rename(boxId: string, musicId: string, name: string): Promise<void> {
    await db.ref(`${this.base(boxId)}/${musicId}`).update({ name, updated_at: Date.now() });
  }

  async removeAndDetach(boxId: string, musicId: string): Promise<number> {
    const alarmsSnap = await db.ref(`boxes/${boxId}/config/alarm_list`).once('value');
    const alarms = (alarmsSnap.val() || {}) as Record<string, { music_id?: string }>;
    const now = Date.now();
    const updates: Record<string, unknown> = {
      [`${this.base(boxId)}/${musicId}`]: null,
      [`boxes/${boxId}/flags/music_flag`]: true,
    };
    let detached = 0;
    for (const [alarmId, a] of Object.entries(alarms)) {
      if (a?.music_id !== musicId) continue;
      updates[`boxes/${boxId}/config/alarm_list/${alarmId}/music_id`] = null;
      updates[`boxes/${boxId}/config/alarm_list/${alarmId}/updated_at`] = now;
      detached++;
    }
    if (detached > 0) updates[`boxes/${boxId}/flags/a_flag`] = true;
    await db.ref().update(updates);
    return detached;
  }
}
