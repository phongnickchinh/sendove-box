"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.BoxService = void 0;
const firebase_box_repository_1 = require("../repositories/firebase/firebase-box.repository");
const firebase_user_repository_1 = require("../repositories/firebase/firebase-user.repository");
const error_handler_middleware_1 = require("../middleware/error-handler.middleware");
// Import the submodule; do NOT use `admin.database.ServerValue` via `import * as admin`:
// with firebase-admin 12 it is undefined at runtime.
const database_1 = require("firebase-admin/database");
class BoxService {
    constructor(boxRepo = new firebase_box_repository_1.FirebaseBoxRepository(), userRepo = new firebase_user_repository_1.FirebaseUserRepository()) {
        this.boxRepo = boxRepo;
        this.userRepo = userRepo;
    }
    /** Pairing by code: scode starts with 'S', rcode with 'R'. */
    async pairBox(uid, pairingCode, boxName) {
        const isSender = pairingCode.startsWith('S');
        const codeType = isSender ? 'scode' : 'rcode';
        const role = isSender ? 'sender' : 'receiver';
        const box = await this.boxRepo.findByPairingCode(pairingCode, codeType);
        if (!box) {
            throw new error_handler_middleware_1.AppError(404, 'box_not_found', 'Invalid pairing code');
        }
        // Default to empty object if Firebase omitted it
        const pairing = box.pairing || {};
        // The slot must be free
        if (isSender && pairing.sender_id) {
            throw new error_handler_middleware_1.AppError(400, 'slot_full', 'Sender slot is already taken');
        }
        if (!isSender && pairing.receiver_id) {
            throw new error_handler_middleware_1.AppError(400, 'slot_full', 'Receiver slot is already taken');
        }
        // A user can't be both sender and receiver of the same box
        const isAlreadyReceiver = isSender && (pairing.receiver_id === uid);
        const isAlreadySender = !isSender && (pairing.sender_id === uid);
        if (isAlreadyReceiver || isAlreadySender) {
            throw new error_handler_middleware_1.AppError(400, 'conflict_role', 'You cannot be both sender and receiver for the same box');
        }
        const now = Date.now();
        // Update the box's pairing
        const pairingUpdate = isSender
            ? { 'pairing/sender_id': uid, 'pairing/sender_paired_time': now }
            : { 'pairing/receiver_id': uid, 'pairing/receiver_paired_time': now };
        await this.boxRepo.update(box.id, { ...pairingUpdate, updated_at: now });
        // Set p_flag so the ESP32 knows the pairing changed
        await this.boxRepo.updateFlags(box.id, { p_flag: true });
        // Update the user's boxes_list
        await this.userRepo.linkBox(uid, box.id, { role, box_name: boxName });
        return { boxId: box.id, role };
    }
    /** Unpair: detach the user from the box. */
    async unpairBox(uid, boxId) {
        const box = await this.boxRepo.getById(boxId);
        if (!box)
            throw new error_handler_middleware_1.AppError(404, 'box_not_found', 'Box not found');
        const pairing = box.pairing || {};
        let roleToUnpair = null;
        if (pairing.sender_id === uid)
            roleToUnpair = 'sender';
        if (pairing.receiver_id === uid)
            roleToUnpair = 'receiver';
        if (!roleToUnpair) {
            throw new error_handler_middleware_1.AppError(403, 'unauthorized', 'You are not paired to this box');
        }
        const now = Date.now();
        // Update the box
        const pairingUpdate = roleToUnpair === 'sender'
            ? { 'pairing/sender_id': null, 'pairing/sender_paired_time': null }
            : { 'pairing/receiver_id': null, 'pairing/receiver_paired_time': null };
        await this.boxRepo.update(boxId, { ...pairingUpdate, updated_at: now });
        // Set p_flag
        await this.boxRepo.updateFlags(boxId, { p_flag: true });
        // Update the user
        await this.userRepo.unlinkBox(uid, boxId);
        return roleToUnpair;
    }
    /** Box details (paired users only), without device_secret and the Wi-Fi password. */
    async getBoxDetails(uid, boxId) {
        const box = await this.boxRepo.getById(boxId);
        if (!box)
            throw new error_handler_middleware_1.AppError(404, 'box_not_found', 'Box not found');
        const pairing = box.pairing || {};
        if (pairing.sender_id !== uid && pairing.receiver_id !== uid) {
            throw new error_handler_middleware_1.AppError(403, 'unauthorized', 'You are not paired to this box');
        }
        // Sanitize: strip sensitive fields before returning from the API
        const { device_secret, config, ...rest } = box;
        const safeConfig = config ? {
            ...config,
            wifi_config: config.wifi_config
                ? { ssid: config.wifi_config.ssid }
                : undefined,
        } : undefined;
        return { ...rest, config: safeConfig };
    }
    /**
     * Update the box's Wi-Fi config. pwd undefined = KEEP the stored password (the
     * web can't read it back to resend it); pwd "" = an open network.
     */
    async updateWifi(uid, boxId, ssid, pwd) {
        await this.getBoxDetails(uid, boxId); // Validates ownership
        const updates = { updated_at: Date.now() };
        if (pwd === undefined) {
            updates['config/wifi_config/ssid'] = ssid;
        }
        else {
            updates['config/wifi_config'] = { ssid, pwd };
        }
        await this.boxRepo.update(boxId, updates);
    }
    /** Update led_state / display_brightness / playback_volume (only the fields passed in). */
    async updateBoxConfig(uid, boxId, data) {
        await this.getBoxDetails(uid, boxId); // Validates ownership
        const updates = { updated_at: Date.now() };
        if (data.led_state !== undefined)
            updates['config/led_state'] = data.led_state;
        if (data.display_brightness !== undefined)
            updates['config/display_brightness'] = data.display_brightness;
        if (data.playback_volume !== undefined)
            updates['config/playback_volume'] = data.playback_volume;
        // The box echoes this to status/config_rev once applied. Incremented atomically
        // on the server, so two close saves never share a rev.
        updates['config/config_rev'] = database_1.ServerValue.increment(1);
        await this.boxRepo.update(boxId, updates);
        // Set config_flag so the ESP32 re-reads its config
        await this.boxRepo.updateFlags(boxId, { config_flag: true });
    }
}
exports.BoxService = BoxService;
//# sourceMappingURL=box.service.js.map