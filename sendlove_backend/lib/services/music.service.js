"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.MusicService = exports.crc32 = void 0;
const firebase_music_repository_1 = require("../repositories/firebase/firebase-music.repository");
const firebase_storage_repository_1 = require("../repositories/firebase/firebase-storage.repository");
const error_handler_middleware_1 = require("../middleware/error-handler.middleware");
const music_types_1 = require("../types/music.types");
const crc32_1 = require("../utils/crc32");
Object.defineProperty(exports, "crc32", { enumerable: true, get: function () { return crc32_1.crc32; } });
// Thay cho thư viện giả (2 bài cứng) trước 2026-09-24. Thiết kế: MEMORY.md §28.
const ID_PATTERN = /^mus_\d{10,16}$/;
/**
 * Kiểm file đúng định dạng hộp phát được: "AUDC" + u16 tần số (LE) + u32 cỡ PCM, rồi
 * RIFF/WAVE 44 byte (AudioPlayer::parseAudc). Sai là hộp im -> chặn ngay ở đây.
 */
function checkAudcFile(buf) {
    if (buf.length < 54 || buf.toString('ascii', 0, 4) !== 'AUDC') {
        throw new error_handler_middleware_1.AppError(400, 'invalid_music', 'File must start with an AUDC header');
    }
    const rate = buf.readUInt16LE(4);
    if (rate !== music_types_1.MUSIC_LIMITS.SAMPLE_RATE) {
        throw new error_handler_middleware_1.AppError(400, 'invalid_music', `Sample rate must be ${music_types_1.MUSIC_LIMITS.SAMPLE_RATE}, got ${rate}`);
    }
    if (buf.toString('ascii', 10, 14) !== 'RIFF' || buf.toString('ascii', 18, 22) !== 'WAVE') {
        throw new error_handler_middleware_1.AppError(400, 'invalid_music', 'AUDC payload must be a WAV file');
    }
}
class MusicService {
    constructor(musicRepo = new firebase_music_repository_1.FirebaseMusicRepository(), storageRepo = new firebase_storage_repository_1.FirebaseStorageRepository()) {
        this.musicRepo = musicRepo;
        this.storageRepo = storageRepo;
        this.pathFor = (boxId, musicId, rev) => `media/${boxId}/music/${musicId}_r${rev}.aud`;
    }
    async list(boxId) {
        return this.musicRepo.list(boxId);
    }
    async exists(boxId, musicId) {
        return ID_PATTERN.test(musicId) && (await this.musicRepo.get(boxId, musicId)) !== null;
    }
    /**
     * Bước 1: cấp signed POST policy. musicId có = thay nội dung bài đó (rev + 1), không có =
     * bài mới. CHƯA ghi gì vào DB: tải lên hỏng giữa chừng thì không có bản ghi mồ côi trỏ
     * tới file không tồn tại (hộp sẽ tải hỏng mãi).
     */
    async initiateUpload(boxId, musicId) {
        let id = musicId;
        let rev = 1;
        if (id) {
            if (!ID_PATTERN.test(id))
                throw new error_handler_middleware_1.AppError(400, 'invalid_music', 'Invalid music_id');
            const existing = await this.musicRepo.get(boxId, id);
            if (!existing)
                throw new error_handler_middleware_1.AppError(404, 'music_not_found', 'Music not found');
            rev = existing.rev + 1;
        }
        else {
            const all = await this.musicRepo.list(boxId);
            if (all.length >= music_types_1.MUSIC_LIMITS.MAX_TRACKS) {
                throw new error_handler_middleware_1.AppError(400, 'music_limit_reached', `Maximum ${music_types_1.MUSIC_LIMITS.MAX_TRACKS} tracks per box`);
            }
            id = `mus_${Date.now()}`;
        }
        const path = this.pathFor(boxId, id, rev);
        const upload = await this.storageRepo.generateUploadPolicy(path, 'application/octet-stream', music_types_1.MUSIC_LIMITS.MAX_BYTES, 15);
        return { music_id: id, rev, path, upload };
    }
    /**
     * Bước 2: file đã lên Storage. Backend TỰ tải về để đo size + crc32 và kiểm header,
     * không tin con số web gửi: hộp dựa vào đúng hai số này để nhận file.
     */
    async commit(boxId, uid, body) {
        const id = String(body?.music_id || '');
        const rev = Number(body?.rev);
        const name = typeof body?.name === 'string' ? body.name.trim() : '';
        const durationMs = Number(body?.duration_ms);
        if (!ID_PATTERN.test(id))
            throw new error_handler_middleware_1.AppError(400, 'invalid_music', 'Invalid music_id');
        if (!Number.isInteger(rev) || rev < 1)
            throw new error_handler_middleware_1.AppError(400, 'invalid_music', 'Invalid rev');
        if (name.length < 1 || name.length > music_types_1.MUSIC_LIMITS.NAME_MAX) {
            throw new error_handler_middleware_1.AppError(400, 'invalid_music', `'name' must be 1-${music_types_1.MUSIC_LIMITS.NAME_MAX} characters`);
        }
        if (!Number.isFinite(durationMs) || durationMs < music_types_1.MUSIC_LIMITS.MIN_MS || durationMs > music_types_1.MUSIC_LIMITS.MAX_MS + 500) {
            throw new error_handler_middleware_1.AppError(400, 'invalid_music', 'Duration must be 5-60 seconds');
        }
        const existing = await this.musicRepo.get(boxId, id);
        if (existing ? rev !== existing.rev + 1 : rev !== 1) {
            throw new error_handler_middleware_1.AppError(409, 'stale_music', 'Music changed meanwhile, upload again');
        }
        if (!existing && (await this.musicRepo.list(boxId)).length >= music_types_1.MUSIC_LIMITS.MAX_TRACKS) {
            throw new error_handler_middleware_1.AppError(400, 'music_limit_reached', `Maximum ${music_types_1.MUSIC_LIMITS.MAX_TRACKS} tracks per box`);
        }
        const path = this.pathFor(boxId, id, rev);
        if (!(await this.storageRepo.fileExists(path))) {
            throw new error_handler_middleware_1.AppError(400, 'invalid_music', 'Uploaded file not found');
        }
        const buf = await this.storageRepo.downloadToBuffer(path);
        try {
            if (buf.length > music_types_1.MUSIC_LIMITS.MAX_BYTES) {
                throw new error_handler_middleware_1.AppError(400, 'invalid_music', `File must be at most ${music_types_1.MUSIC_LIMITS.MAX_BYTES} bytes`);
            }
            checkAudcFile(buf);
        }
        catch (err) {
            await this.storageRepo.deleteFile(path).catch(() => undefined);
            throw err;
        }
        const now = Date.now();
        const music = {
            id,
            music_id: id,
            name,
            rev,
            duration_ms: Math.round(durationMs),
            sample_rate: music_types_1.MUSIC_LIMITS.SAMPLE_RATE,
            size: buf.length,
            crc32: (0, crc32_1.crc32)(buf),
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
    async rename(boxId, musicId, name) {
        const n = typeof name === 'string' ? name.trim() : '';
        if (n.length < 1 || n.length > music_types_1.MUSIC_LIMITS.NAME_MAX) {
            throw new error_handler_middleware_1.AppError(400, 'invalid_music', `'name' must be 1-${music_types_1.MUSIC_LIMITS.NAME_MAX} characters`);
        }
        if (!(await this.exists(boxId, musicId)))
            throw new error_handler_middleware_1.AppError(404, 'music_not_found', 'Music not found');
        await this.musicRepo.rename(boxId, musicId, n);
    }
    /** Xoá bài; báo thức đang dùng nó chuyển về tiếng bíp trong cùng một lần ghi. */
    async remove(boxId, musicId) {
        const existing = ID_PATTERN.test(musicId) ? await this.musicRepo.get(boxId, musicId) : null;
        if (!existing)
            throw new error_handler_middleware_1.AppError(404, 'music_not_found', 'Music not found');
        const detached = await this.musicRepo.removeAndDetach(boxId, musicId);
        await this.storageRepo.deleteFile(existing.storage_path).catch(() => undefined);
        return { detached_alarms: detached };
    }
    /** URL ký 15 phút để web nghe lại bài đã lưu (web tự bỏ 10 byte AUDC trước RIFF). */
    async previewUrl(boxId, musicId) {
        const existing = ID_PATTERN.test(musicId) ? await this.musicRepo.get(boxId, musicId) : null;
        if (!existing)
            throw new error_handler_middleware_1.AppError(404, 'music_not_found', 'Music not found');
        return this.storageRepo.generateDownloadUrl(existing.storage_path, 15);
    }
}
exports.MusicService = MusicService;
//# sourceMappingURL=music.service.js.map