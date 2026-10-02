/**
 * Reads boxes/{id}/status, which has TWO writers: the backend heartbeat
 * (last_seen in milliseconds, fw_version, charging) and the firmware itself
 * (last_seen in SECONDS, "fw", "is_charging").
 */

/** 1e12 ms = year 2001; an epoch in seconds stays below that until year 33658. */
export function lastSeenMs(status) {
  const t = status?.last_seen;
  if (!t) return null;
  return t < 1e12 ? t * 1000 : t;
}

export const fwVersion = (status) => status?.fw_version || status?.fw || null;

const MINUTE = 60 * 1000;

/**
 * Connection health from last_seen — do NOT use status.online (the box mostly
 * sleeps). It wakes every ~5 minutes: 15 minutes late is normal, 2 hours = lost.
 */
export function syncTone(seenAtMs) {
  if (!seenAtMs) return 'unknown';
  const diff = Date.now() - seenAtMs;
  if (diff < 15 * MINUTE) return 'ok';
  if (diff < 2 * 60 * MINUTE) return 'late';
  return 'lost';
}

/**
 * Video/audio duration cap by storage type: NAND (3 slots of ~5.3 MB) → 15s;
 * SD → 60s, the backend's cap. No firmware sends storage_type yet; a missing
 * value counts as SD, the current build (product decision).
 */
export const MAX_SECONDS = { nand: 15, sd: 60 };

export function maxSecondsFor(box) {
  return box?.status?.storage_type === 'nand' ? MAX_SECONDS.nand : MAX_SECONDS.sd;
}

/** Max bin file size the backend accepts (message.service.ts initiateMessage). */
export const MAX_BIN_BYTES = 25 * 1024 * 1024;
