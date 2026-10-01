import { BaseModel } from './base.types';

// ==================================================
// Message — Node: messages/{box_id}/{message_id}
// ==================================================
// There is no MessageStatus: by design the sender doesn't know a message's state.
// The ESP32 finds new messages from timestamp + its local last_download_ts.
// ==================================================
export interface Message extends BaseModel {
  sender_id: string;
  box_id: string;

  /** Send time (the ESP32 compares it with its local last_download_ts) */
  timestamp: number;

  /** Primary message type */
  type: 'video' | 'image' | 'gif' | 'voice' | 'text';

  /** Text content */
  text?: string;

  /** Encoded visual (.bin) — Firebase Storage path */
  bin_url?: string;

  /** Recorded audio (.wav) — Firebase Storage path */
  voice_url?: string;

  /** Original file: video (.mp4) */
  video_url?: string;

  /** Original file: GIF */
  gif_url?: string;

  /** Original file: background music */
  bg_music_url?: string;

  /** Original file: still image */
  image_url?: string;

  /** Total size of encoded + original attachments (bytes) */
  total_size?: number;

  /** Thumbnail for the web app's history view */
  thumbnail_url?: string;

  /** Media duration in seconds */
  duration?: number;

  /** Frame count of the .bin file */
  frame_count?: number;

  /** Encoded video dimensions (pixels) */
  width?: number;
  height?: number;
}
