"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.AlarmService = void 0;
const firebase_alarm_repository_1 = require("../repositories/firebase/firebase-alarm.repository");
const error_handler_middleware_1 = require("../middleware/error-handler.middleware");
const music_service_1 = require("./music.service");
/** Âm lượng báo thức mặc định (user chốt 2026-09-24), khớp AlarmItem.volume của firmware. */
const DEFAULT_VOLUME = 80;
class AlarmService {
    constructor(alarmRepo = new firebase_alarm_repository_1.FirebaseAlarmRepository(), musicService = new music_service_1.MusicService()) {
        this.alarmRepo = alarmRepo;
        this.musicService = musicService;
    }
    /** music_id phải là bài có trong thư viện của CHÍNH hộp này. "" -> null (gỡ nhạc). */
    async resolveMusic(boxId, musicId) {
        if (musicId === undefined)
            return undefined;
        if (musicId === '')
            return null;
        if (!(await this.musicService.exists(boxId, musicId))) {
            throw new error_handler_middleware_1.AppError(400, 'music_not_found', 'music_id is not in this box library');
        }
        return musicId;
    }
    /**
     * Tạo alarm mới (max 10 alarms / box).
     * Repository tự set a_flag = true khi tạo.
     */
    async createAlarm(boxId, data) {
        const alarms = await this.alarmRepo.listAlarms(boxId);
        if (alarms.length >= 10) {
            throw new error_handler_middleware_1.AppError(400, 'alarm_limit_reached', 'Maximum 10 alarms allowed');
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
    async getAlarms(boxId) {
        return this.alarmRepo.listAlarms(boxId);
    }
    async updateAlarm(boxId, alarmId, data) {
        const alarm = await this.alarmRepo.getAlarm(boxId, alarmId);
        if (!alarm)
            throw new error_handler_middleware_1.AppError(404, 'alarm_not_found', 'Alarm not found');
        const { music_id, ...rest } = data;
        const musicId = await this.resolveMusic(boxId, music_id);
        return this.alarmRepo.updateAlarm(boxId, alarmId, {
            ...rest,
            // null xoá khoá trong RTDB (update()) -> báo thức về tiếng bíp
            ...(musicId !== undefined ? { music_id: musicId } : {}),
            updated_at: Date.now(),
        });
    }
    async deleteAlarm(boxId, alarmId) {
        const alarm = await this.alarmRepo.getAlarm(boxId, alarmId);
        if (!alarm)
            throw new error_handler_middleware_1.AppError(404, 'alarm_not_found', 'Alarm not found');
        await this.alarmRepo.deleteAlarm(boxId, alarmId);
    }
}
exports.AlarmService = AlarmService;
//# sourceMappingURL=alarm.service.js.map