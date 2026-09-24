import { IAlarmRepository } from '../repositories/interfaces/alarm.repository.interface';
import { FirebaseAlarmRepository } from '../repositories/firebase/firebase-alarm.repository';
import { Alarm } from '../types/alarm.types';
import { AppError } from '../middleware/error-handler.middleware';
import { MusicService } from './music.service';

/** Âm lượng báo thức mặc định (user chốt 2026-09-24), khớp AlarmItem.volume của firmware. */
const DEFAULT_VOLUME = 80;

type AlarmInput = {
  time?: string;
  is_enable?: boolean;
  repeatable?: boolean;
  /** "" = bỏ nhạc, kêu bằng tiếng bíp */
  music_id?: string;
  volume?: number;
  ramp?: boolean;
};

export class AlarmService {
  constructor(
    private alarmRepo: IAlarmRepository = new FirebaseAlarmRepository(),
    private musicService: MusicService = new MusicService()
  ) {}

  /** music_id phải là bài có trong thư viện của CHÍNH hộp này. "" -> null (gỡ nhạc). */
  private async resolveMusic(boxId: string, musicId: string | undefined): Promise<string | null | undefined> {
    if (musicId === undefined) return undefined;
    if (musicId === '') return null;
    if (!(await this.musicService.exists(boxId, musicId))) {
      throw new AppError(400, 'music_not_found', 'music_id is not in this box library');
    }
    return musicId;
  }

  /**
   * Tạo alarm mới (max 10 alarms / box).
   * Repository tự set a_flag = true khi tạo.
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
      // null xoá khoá trong RTDB (update()) -> báo thức về tiếng bíp
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
