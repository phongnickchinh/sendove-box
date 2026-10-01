import { BaseModel } from './base.types';

// ==================================================
// Alarm music — node: boxes/{boxId}/music/{musicId}
// ==================================================
// The library is per BOX and managed by the receiver only (product decision, see
// firmware MEMORY.md §28).
// Storage file: media/{boxId}/music/{musicId}_r{rev}.aud = AUDC(10) + WAV(44) + PCM
// 16 kHz mono 16-bit, packaged by the web (decoding happens on the client).
// Changing the content = new rev + new file; an old file is never overwritten
// (a box still downloading the old revision must not read mixed data).

export interface AlarmMusic extends BaseModel {
  music_id: string;
  name: string;
  /** Bumped whenever the content is replaced. The box compares it with /alarm/index.json on the card. */
  rev: number;
  duration_ms: number;
  sample_rate: number;
  /** Size in bytes of the whole .aud file — the box checks it before renaming the .part */
  size: number;
  /** CRC-32 (zlib) of the whole file, computed by the backend from the uploaded file */
  crc32: number;
  storage_path: string;
  created_by: string;
}

export const MUSIC_LIMITS = {
  MAX_TRACKS: 10,
  MIN_MS: 5000,
  MAX_MS: 60000,
  /** Matches the firmware's ALARM_MUSIC_MAX_BYTES: 60s × 16000 × 2 + header < 2MB */
  MAX_BYTES: 2_000_000,
  SAMPLE_RATE: 16000,
  NAME_MAX: 40,
} as const;
