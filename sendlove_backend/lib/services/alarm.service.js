"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.AlarmService = void 0;
const firebase_alarm_repository_1 = require("../repositories/firebase/firebase-alarm.repository");
const error_handler_middleware_1 = require("../middleware/error-handler.middleware");
class AlarmService {
    constructor() {
        this.alarmRepo = new firebase_alarm_repository_1.FirebaseAlarmRepository();
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
        return this.alarmRepo.createAlarm(boxId, alarmId, {
            time: data.time,
            is_enable: data.is_enable,
            repeatable: data.repeatable,
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
        return this.alarmRepo.updateAlarm(boxId, alarmId, {
            ...data,
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