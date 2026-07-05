"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.AlarmService = void 0;
const firebase_alarm_repository_1 = require("../repositories/firebase/firebase-alarm.repository");
const firebase_box_repository_1 = require("../repositories/firebase/firebase-box.repository");
const error_handler_middleware_1 = require("../middleware/error-handler.middleware");
class AlarmService {
    constructor() {
        this.alarmRepo = new firebase_alarm_repository_1.FirebaseAlarmRepository();
        this.boxRepo = new firebase_box_repository_1.FirebaseBoxRepository();
    }
    async notifyESP32(boxId) {
        await this.boxRepo.updatePollingCache(boxId, { hasNewAlarms: true });
    }
    async createAlarm(uid, boxId, data) {
        // Note: We should ideally verify if uid is the receiver here, but that is typically done in the controller/middleware
        const alarms = await this.alarmRepo.listAlarms(boxId);
        if (alarms.length >= 10) {
            throw new error_handler_middleware_1.AppError(400, 'alarm_limit_reached', 'Maximum 10 alarms allowed');
        }
        const alarmId = `alarm_${Date.now()}`;
        const newAlarm = await this.alarmRepo.createAlarm(boxId, alarmId, {
            ...data,
            createdAt: Date.now(),
        });
        await this.notifyESP32(boxId);
        return newAlarm;
    }
    async getAlarms(boxId) {
        return this.alarmRepo.listAlarms(boxId);
    }
    async updateAlarm(boxId, alarmId, data) {
        const alarm = await this.alarmRepo.getAlarm(boxId, alarmId);
        if (!alarm)
            throw new error_handler_middleware_1.AppError(404, 'alarm_not_found', 'Alarm not found');
        const updated = await this.alarmRepo.updateAlarm(boxId, alarmId, data);
        await this.notifyESP32(boxId);
        return updated;
    }
    async deleteAlarm(boxId, alarmId) {
        const alarm = await this.alarmRepo.getAlarm(boxId, alarmId);
        if (!alarm)
            throw new error_handler_middleware_1.AppError(404, 'alarm_not_found', 'Alarm not found');
        await this.alarmRepo.deleteAlarm(boxId, alarmId);
        await this.notifyESP32(boxId);
    }
}
exports.AlarmService = AlarmService;
//# sourceMappingURL=alarm.service.js.map