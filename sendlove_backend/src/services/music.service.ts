import { IMusicRepository } from '../repositories/interfaces/music.repository.interface';
import { IStorageRepository } from '../repositories/interfaces/storage.repository.interface';
import { FirebaseMusicRepository } from '../repositories/firebase/firebase-music.repository';
import { FirebaseStorageRepository } from '../repositories/firebase/firebase-storage.repository';
import { AppError } from '../middleware/error-handler.middleware';
import { AlarmMusic, MUSIC_LIMITS } from '../types/music.types';
import { crc32 } from '../utils/crc32';

// Design: firmware MEMORY.md §28.

const ID_PATTERN = /^mus_\d{10,16}$/;

export { crc32 };

/**
 * Checks the file is in the format the box can play: "AUDC" + u16 sample rate
 * (LE) + u32 PCM size, then a 44-byte RIFF/WAVE header (AudioPlayer::parseAudc).
 * A bad file makes the box silent -> reject it here.
 */
function checkAudcFile(buf: Buffer): void {
  if (buf.length < 54 || buf.toString('ascii', 0, 4) !== 'AUDC') {
    throw new AppError(400, 'invalid_music', 'File must start with an AUDC header');
  }
  const rate = buf.readUInt16LE(4);
  if (rate !== MUSIC_LIMITS.SAMPLE_RATE) {
    throw new AppError(400, 'invalid_music', `Sample rate must be ${MUSIC_LIMITS.SAMPLE_RATE}, got ${rate}`);
  }
  if (buf.toString('ascii', 10, 14) !== 'RIFF' || buf.toString('ascii', 18, 22) !== 'WAVE') {
    throw new AppError(400, 'invalid_music', 'AUDC payload must be a WAV file');
  }
}

export class MusicService {
  constructor(
    private musicRepo: IMusicRepository = new FirebaseMusicRepository(),
    private storageRepo: IStorageRepository = new FirebaseStorageRepository()
  ) {}

  private pathFor = (boxId: string, musicId: string, rev: number) =>
    `media/${boxId}/music/${musicId}_r${rev}.aud`;

  async list(boxId: string): Promise<AlarmMusic[]> {
    return this.musicRepo.list(boxId);
  }

  async exists(boxId: string, musicId: string): Promise<boolean> {
    return ID_PATTERN.test(musicId) && (await this.musicRepo.get(boxId, musicId)) !== null;
  }

  /**
   * Step 1: issue a signed POST policy. With musicId = replace that track
   * (rev + 1); without = a new track. NOTHING is written to the DB yet: if the
   * upload dies midway there is no orphan record pointing at a missing file
   * (which the box would fail to download forever).
   */
  async initiateUpload(boxId: string, musicId?: string) {
    let id = musicId;
    let rev = 1;
    if (id) {
      if (!ID_PATTERN.test(id)) throw new AppError(400, 'invalid_music', 'Invalid music_id');
      const existing = await this.musicRepo.get(boxId, id);
      if (!existing) throw new AppError(404, 'music_not_found', 'Music not found');
      rev = existing.rev + 1;
    } else {
      const all = await this.musicRepo.list(boxId);
      if (all.length >= MUSIC_LIMITS.MAX_TRACKS) {
        throw new AppError(400, 'music_limit_reached', `Maximum ${MUSIC_LIMITS.MAX_TRACKS} tracks per box`);
      }
      id = `mus_${Date.now()}`;
    }
    const path = this.pathFor(boxId, id, rev);
    const upload = await this.storageRepo.generateUploadPolicy(
      path, 'application/octet-stream', MUSIC_LIMITS.MAX_BYTES, 15);
    return { music_id: id, rev, path, upload };
  }

  /**
   * Step 2: the file is in Storage. The backend downloads it ITSELF to measure
   * size + crc32 and check the header, never trusting values sent by the web:
   * the box relies on exactly these two numbers to accept the file.
   */
  async commit(boxId: string, uid: string, body: any): Promise<AlarmMusic> {
    const id = String(body?.music_id || '');
    const rev = Number(body?.rev);
    const name = typeof body?.name === 'string' ? body.name.trim() : '';
    const durationMs = Number(body?.duration_ms);

    if (!ID_PATTERN.test(id)) throw new AppError(400, 'invalid_music', 'Invalid music_id');
    if (!Number.isInteger(rev) || rev < 1) throw new AppError(400, 'invalid_music', 'Invalid rev');
    if (name.length < 1 || name.length > MUSIC_LIMITS.NAME_MAX) {
      throw new AppError(400, 'invalid_music', `'name' must be 1-${MUSIC_LIMITS.NAME_MAX} characters`);
    }
    if (!Number.isFinite(durationMs) || durationMs < MUSIC_LIMITS.MIN_MS || durationMs > MUSIC_LIMITS.MAX_MS + 500) {
      throw new AppError(400, 'invalid_music', 'Duration must be 5-60 seconds');
    }

    const existing = await this.musicRepo.get(boxId, id);
    if (existing ? rev !== existing.rev + 1 : rev !== 1) {
      throw new AppError(409, 'stale_music', 'Music changed meanwhile, upload again');
    }
    if (!existing && (await this.musicRepo.list(boxId)).length >= MUSIC_LIMITS.MAX_TRACKS) {
      throw new AppError(400, 'music_limit_reached', `Maximum ${MUSIC_LIMITS.MAX_TRACKS} tracks per box`);
    }

    const path = this.pathFor(boxId, id, rev);
    if (!(await this.storageRepo.fileExists(path))) {
      throw new AppError(400, 'invalid_music', 'Uploaded file not found');
    }
    const buf = await this.storageRepo.downloadToBuffer(path);
    try {
      if (buf.length > MUSIC_LIMITS.MAX_BYTES) {
        throw new AppError(400, 'invalid_music', `File must be at most ${MUSIC_LIMITS.MAX_BYTES} bytes`);
      }
      checkAudcFile(buf);
    } catch (err) {
      await this.storageRepo.deleteFile(path).catch(() => undefined);
      throw err;
    }

    const now = Date.now();
    const music: AlarmMusic = {
      id,
      music_id: id,
      name,
      rev,
      duration_ms: Math.round(durationMs),
      sample_rate: MUSIC_LIMITS.SAMPLE_RATE,
      size: buf.length,
      crc32: crc32(buf),
      storage_path: path,
      created_by: existing?.created_by || uid,
      created_at: existing?.created_at || now,
      updated_at: now,
    };
    await this.musicRepo.save(boxId, music);
    if (existing && existing.storage_path !== path) {
      await this.storageRepo.deleteFile(existing.storage_path).catch(() => undefined);
    }
    return music;
  }

  async rename(boxId: string, musicId: string, name: unknown): Promise<void> {
    const n = typeof name === 'string' ? name.trim() : '';
    if (n.length < 1 || n.length > MUSIC_LIMITS.NAME_MAX) {
      throw new AppError(400, 'invalid_music', `'name' must be 1-${MUSIC_LIMITS.NAME_MAX} characters`);
    }
    if (!(await this.exists(boxId, musicId))) throw new AppError(404, 'music_not_found', 'Music not found');
    await this.musicRepo.rename(boxId, musicId, n);
  }

  /** Delete a track; alarms using it switch to the beep in the same write. */
  async remove(boxId: string, musicId: string): Promise<{ detached_alarms: number }> {
    const existing = ID_PATTERN.test(musicId) ? await this.musicRepo.get(boxId, musicId) : null;
    if (!existing) throw new AppError(404, 'music_not_found', 'Music not found');
    const detached = await this.musicRepo.removeAndDetach(boxId, musicId);
    await this.storageRepo.deleteFile(existing.storage_path).catch(() => undefined);
    return { detached_alarms: detached };
  }

  /** 15-minute signed URL so the web can play a stored track (the web skips the 10 AUDC bytes before RIFF). */
  async previewUrl(boxId: string, musicId: string): Promise<string> {
    const existing = ID_PATTERN.test(musicId) ? await this.musicRepo.get(boxId, musicId) : null;
    if (!existing) throw new AppError(404, 'music_not_found', 'Music not found');
    return this.storageRepo.generateDownloadUrl(existing.storage_path, 15);
  }
}
