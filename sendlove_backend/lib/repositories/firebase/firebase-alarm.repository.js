"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.FirebaseAlarmRepository = void 0;
const firebase_1 = require("../../firebase");
/**
 * Alarm được lưu tại: boxes/{boxId}/config/alarm_list/{alarmId}
 * Khi thay đổi alarm list, cần set flags/a_flag = true để ESP32 biết.
 */
class FirebaseAlarmRepository {
    getBasePath(boxId) {
        return `boxes/${boxId}/config/alarm_list`;
    }
    async createAlarm(boxId, alarmId, data) {
        const ref = firebase_1.db.ref(`${this.getBasePath(boxId)}/${alarmId}`);
        const record = { id: alarmId, ...data };
        await ref.set(record);
        // Set a_flag = true để ESP32 biết alarm list đã thay đổi
        await firebase_1.db.ref(`boxes/${boxId}/flags/a_flag`).set(true);
        return record;
    }
    async getAlarm(boxId, alarmId) {
        const snapshot = await firebase_1.db.ref(`${this.getBasePath(boxId)}/${alarmId}`).once('value');
        if (!snapshot.exists())
            return null;
        return snapshot.val();
    }
    async updateAlarm(boxId, alarmId, data) {
        const ref = firebase_1.db.ref(`${this.getBasePath(boxId)}/${alarmId}`);
        await ref.update(data);
        // Set a_flag = true
        await firebase_1.db.ref(`boxes/${boxId}/flags/a_flag`).set(true);
        return this.getAlarm(boxId, alarmId);
    }
    async deleteAlarm(boxId, alarmId) {
        await firebase_1.db.ref(`${this.getBasePath(boxId)}/${alarmId}`).remove();
        // Set a_flag = true
        await firebase_1.db.ref(`boxes/${boxId}/flags/a_flag`).set(true);
    }
    async listAlarms(boxId) {
        const snapshot = await firebase_1.db.ref(this.getBasePath(boxId)).once('value');
        if (!snapshot.exists())
            return [];
        const alarmsObj = snapshot.val();
        const alarms = [];
        for (const id in alarmsObj) {
            alarms.push(alarmsObj[id]);
        }
        return alarms;
    }
}
exports.FirebaseAlarmRepository = FirebaseAlarmRepository;
//# sourceMappingURL=firebase-alarm.repository.js.map