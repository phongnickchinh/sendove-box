import { AlarmMusic } from '../../types/music.types';

export interface IMusicRepository {
  list(boxId: string): Promise<AlarmMusic[]>;
  get(boxId: string, musicId: string): Promise<AlarmMusic | null>;
  /** Write the record + set flags/music_flag so the box refetches the music list. */
  save(boxId: string, music: AlarmMusic): Promise<void>;
  /** Rename only: the box doesn't need to know -> no flag. */
  rename(boxId: string, musicId: string, name: string): Promise<void>;
  /**
   * Delete a track and clear music_id on every alarm using it, IN ONE multi-path
   * write. Sets music_flag (and a_flag if an alarm changed). Returns the number
   * of alarms detached.
   */
  removeAndDetach(boxId: string, musicId: string): Promise<number>;
}
