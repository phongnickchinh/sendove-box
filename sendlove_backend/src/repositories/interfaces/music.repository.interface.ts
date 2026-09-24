import { AlarmMusic } from '../../types/music.types';

export interface IMusicRepository {
  list(boxId: string): Promise<AlarmMusic[]>;
  get(boxId: string, musicId: string): Promise<AlarmMusic | null>;
  /** Ghi bản ghi + bật flags/music_flag để hộp lấy lại danh sách nhạc. */
  save(boxId: string, music: AlarmMusic): Promise<void>;
  /** Chỉ đổi tên: hộp không cần biết -> không bật cờ. */
  rename(boxId: string, musicId: string, name: string): Promise<void>;
  /**
   * Xoá bài + gỡ music_id khỏi mọi báo thức đang dùng nó, TRONG MỘT lần ghi nhiều đường
   * (không bao giờ có lúc báo thức trỏ tới bài đã xoá). Bật music_flag, và a_flag nếu có
   * báo thức bị đổi. Trả số báo thức đã gỡ.
   */
  removeAndDetach(boxId: string, musicId: string): Promise<number>;
}
