import { BaseModel } from './base.types';

// Alarm — boxes/{boxId}/config/alarm_list/{alarmId}
export interface Alarm extends BaseModel {
  /** Ring time, "HH:mm" (24h) */
  time: string;

  /** Whether this alarm is on */
  is_enable: boolean;

  /** true = repeats daily; false = one-shot (the box sets is_enable = false after ringing). */
  repeatable: boolean;

  /** A track in boxes/{boxId}/music. Absent = beep. The box only downloads tracks that alarms use. */
  music_id?: string | null;
  /** 0-100, per alarm (default 80). The web blocks < 20. */
  volume?: number;
  /** Fade in from 30% of the chosen level over 20s (on by default). */
  ramp?: boolean;
}
