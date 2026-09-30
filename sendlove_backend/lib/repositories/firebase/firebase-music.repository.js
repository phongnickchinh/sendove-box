"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.FirebaseMusicRepository = void 0;
const firebase_1 = require("../../firebase");
/** boxes/{boxId}/music/{musicId} — xem music.types.ts */
class FirebaseMusicRepository {
    constructor() {
        this.base = (boxId) => `boxes/${boxId}/music`;
    }
    async list(boxId) {
        const snap = await firebase_1.db.ref(this.base(boxId)).once('value');
        const val = (snap.val() || {});
        return Object.values(val).sort((a, b) => (a.created_at || 0) - (b.created_at || 0));
    }
    async get(boxId, musicId) {
        const snap = await firebase_1.db.ref(`${this.base(boxId)}/${musicId}`).once('value');
        return snap.exists() ? snap.val() : null;
    }
    async save(boxId, music) {
        await firebase_1.db.ref().update({
            [`${this.base(boxId)}/${music.music_id}`]: music,
            [`boxes/${boxId}/flags/music_flag`]: true,
        });
    }
    async rename(boxId, musicId, name) {
        await firebase_1.db.ref(`${this.base(boxId)}/${musicId}`).update({ name, updated_at: Date.now() });
    }
    async removeAndDetach(boxId, musicId) {
        const alarmsSnap = await firebase_1.db.ref(`boxes/${boxId}/config/alarm_list`).once('value');
        const alarms = (alarmsSnap.val() || {});
        const now = Date.now();
        const updates = {
            [`${this.base(boxId)}/${musicId}`]: null,
            [`boxes/${boxId}/flags/music_flag`]: true,
        };
        let detached = 0;
        for (const [alarmId, a] of Object.entries(alarms)) {
            if (a?.music_id !== musicId)
                continue;
            updates[`boxes/${boxId}/config/alarm_list/${alarmId}/music_id`] = null;
            updates[`boxes/${boxId}/config/alarm_list/${alarmId}/updated_at`] = now;
            detached++;
        }
        if (detached > 0)
            updates[`boxes/${boxId}/flags/a_flag`] = true;
        await firebase_1.db.ref().update(updates);
        return detached;
    }
}
exports.FirebaseMusicRepository = FirebaseMusicRepository;
//# sourceMappingURL=firebase-music.repository.js.map