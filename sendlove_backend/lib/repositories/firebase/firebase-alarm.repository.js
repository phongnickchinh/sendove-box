"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.FirebaseAlarmRepository = void 0;
const firebase_1 = require("../../firebase");
class FirebaseAlarmRepository {
    // Alarms are stored under `alarms/{boxId}/{alarmId}`
    async createAlarm(boxId, alarmId, data) {
        const ref = firebase_1.db.ref(`alarms/${boxId}/${alarmId}`);
        await ref.set(data);
        return this.getAlarm(boxId, alarmId);
    }
    async getAlarm(boxId, alarmId) {
        const snapshot = await firebase_1.db.ref(`alarms/${boxId}/${alarmId}`).once('value');
        if (!snapshot.exists())
            return null;
        return { alarmId, ...snapshot.val() };
    }
    async updateAlarm(boxId, alarmId, data) {
        const ref = firebase_1.db.ref(`alarms/${boxId}/${alarmId}`);
        await ref.update(data);
        return this.getAlarm(boxId, alarmId);
    }
    async deleteAlarm(boxId, alarmId) {
        const ref = firebase_1.db.ref(`alarms/${boxId}/${alarmId}`);
        await ref.remove();
    }
    async listAlarms(boxId) {
        const snapshot = await firebase_1.db.ref(`alarms/${boxId}`).once('value');
        if (!snapshot.exists())
            return [];
        const alarmsObj = snapshot.val();
        const alarms = [];
        for (const id in alarmsObj) {
            alarms.push({ alarmId: id, ...alarmsObj[id] });
        }
        return alarms;
    }
}
exports.FirebaseAlarmRepository = FirebaseAlarmRepository;
//# sourceMappingURL=firebase-alarm.repository.js.map