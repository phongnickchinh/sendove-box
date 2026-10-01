import { IAlarmRepository } from '../repositories/interfaces/alarm.repository.interface';
import { FirebaseAlarmRepository } from '../repositories/firebase/firebase-alarm.repository';
import { Alarm } from '../types/alarm.types';
import { AppError } from '../middleware/error-handler.middleware';
import { MusicService } from './music.service';

/** Default alarm volume (product decision), matching the firmware's AlarmItem.volume. */
const DEFAULT_VOLUME = 80;

type AlarmInput = {
  time?: string;
  is_enable?: boolean;
  repeatable?: boolean;
  /** "" = remove the music and ring with the beep */
  music_id?: string;
  volume?: number;
  ramp?: boolean;
};

export class AlarmService {
  constructor(
    private alarmRepo: IAlarmRepository = new FirebaseAlarmRepository(),
    private musicService: MusicService = new MusicService()
  ) {}

  /** music_id must be a track in THIS box's library. "" -> null (remove the music). */
  private async resolveMusic(boxId: string, musicId: string | undefined): Promise<string | null | undefined> {
    if (musicId === undefined) return undefined;
    if (musicId === '') return null;
    if (!(await this.musicService.exists(boxId, musicId))) {
      throw new AppError(400, 'music_not_found', 'music_id is not in this box library');
    }
    return musicId;
  }

  /**
   * Create an alarm (max 10 per box).
   * The repository sets a_flag = true on create.
   */
  async createAlarm(boxId: string, data: AlarmInput & { time: string; is_enable: boolean; repeatable: boolean }): Promise<Alarm> {
    const alarms = await this.alarmRepo.listAlarms(boxId);
    if (alarms.length >= 10) {
      throw new AppError(400, 'alarm_limit_reached', 'Maximum 10 alarms allowed');
    }

    const now = Date.now();
    const alarmId = `alarm_${now}`;
    const musicId = await this.resolveMusic(boxId, data.music_id);

    return this.alarmRepo.createAlarm(boxId, alarmId, {
      time: data.time,
      is_enable: data.is_enable,
      repeatable: data.repeatable,
      ...(musicId ? { music_id: musicId } : {}),
      volume: data.volume ?? DEFAULT_VOLUME,
      ramp: data.ramp ?? true,
      created_at: now,
      updated_at: now,
    });
  }

  async getAlarms(boxId: string): Promise<Alarm[]> {
    return this.alarmRepo.listAlarms(boxId);
  }

  async updateAlarm(boxId: string, alarmId: string, data: AlarmInput): Promise<Alarm> {
    const alarm = await this.alarmRepo.getAlarm(boxId, alarmId);
    if (!alarm) throw new AppError(404, 'alarm_not_found', 'Alarm not found');

    const { music_id, ...rest } = data;
    const musicId = await this.resolveMusic(boxId, music_id);
    return this.alarmRepo.updateAlarm(boxId, alarmId, {
      ...rest,
      // null deletes the key in RTDB (update()) -> the alarm falls back to the beep
      ...(musicId !== undefined ? { music_id: musicId } : {}),
      updated_at: Date.now(),
    } as Partial<Alarm>);
  }

  async deleteAlarm(boxId: string, alarmId: string): Promise<void> {
    const alarm = await this.alarmRepo.getAlarm(boxId, alarmId);
    if (!alarm) throw new AppError(404, 'alarm_not_found', 'Alarm not found');

    await this.alarmRepo.deleteAlarm(boxId, alarmId);
  }
}
