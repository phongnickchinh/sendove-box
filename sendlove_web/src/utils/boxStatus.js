/**
 * Reads boxes/{id}/status correctly. The node has TWO writers:
 *   - backend /device/heartbeat: last_seen in milliseconds, fw_version, charging
 *   - the current firmware PATCHes status.json directly (NetworkManager.cpp heartbeat):
 *     last_seen = time(nullptr) in SECONDS, "fw", "is_charging"
 * Reading the firmware's last_seen as milliseconds yields "synced 20,000 days ago".
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
 * Connection health derived from last_seen — do NOT use status.online (the box
 * sleeps almost all the time; online=false doesn't mean broken). The box wakes
 * every ~5 minutes: up to 15 minutes late is still normal, and only past 2
 * hours is it worth calling lost.
 */
export function syncTone(seenAtMs) {
  if (!seenAtMs) return 'unknown';
  const diff = Date.now() - seenAtMs;
  if (diff < 15 * MINUTE) return 'ok';
  if (diff < 2 * 60 * MINUTE) return 'late';
  return 'lost';
}

/**
 * Video/audio duration cap by the box's storage type.
 * NAND: 3 fixed ~5.3 MB slots → 15s.
 * SD card, or a box that hasn't reported its storage (no firmware sends
 * storage_type yet; product decision: treat a missing value as SD, the current
 * build) → 60s, the backend's
 * duration cap (validation.middleware.ts confirmMessageSchema).
 */
export const MAX_SECONDS = { nand: 15, sd: 60 };

export function maxSecondsFor(box) {
  return box?.status?.storage_type === 'nand' ? MAX_SECONDS.nand : MAX_SECONDS.sd;
}

/** Max bin file size the backend accepts (message.service.ts initiateMessage). */
export const MAX_BIN_BYTES = 25 * 1024 * 1024;
