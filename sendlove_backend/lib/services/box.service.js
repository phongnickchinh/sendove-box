"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.BoxService = void 0;
const firebase_box_repository_1 = require("../repositories/firebase/firebase-box.repository");
const firebase_user_repository_1 = require("../repositories/firebase/firebase-user.repository");
const error_handler_middleware_1 = require("../middleware/error-handler.middleware");
class BoxService {
    constructor() {
        this.boxRepo = new firebase_box_repository_1.FirebaseBoxRepository();
        this.userRepo = new firebase_user_repository_1.FirebaseUserRepository();
    }
    async pairBox(uid, pairingCode) {
        const box = await this.boxRepo.findByPairingCode(pairingCode);
        if (!box) {
            throw new error_handler_middleware_1.AppError(404, 'box_not_found', 'Invalid pairing code');
        }
        const isSender = pairingCode.startsWith('SCODE');
        const role = isSender ? 'sender' : 'receiver';
        if (isSender && box.pairingInfo.senderUid) {
            throw new error_handler_middleware_1.AppError(400, 'slot_full', 'Sender slot is already taken');
        }
        if (!isSender && box.pairingInfo.receiverUid) {
            throw new error_handler_middleware_1.AppError(400, 'slot_full', 'Receiver slot is already taken');
        }
        // Check if the user is already paired to this box with the other role
        if ((isSender && box.pairingInfo.receiverUid === uid) || (!isSender && box.pairingInfo.senderUid === uid)) {
            throw new error_handler_middleware_1.AppError(400, 'conflict_role', 'You cannot be both sender and receiver for the same box');
        }
        // Update Box
        const updateData = isSender ? { 'pairingInfo/senderUid': uid } : { 'pairingInfo/receiverUid': uid };
        await this.boxRepo.update(box.boxId, updateData);
        // Update User
        await this.userRepo.linkBox(uid, box.boxId, role);
        return { boxId: box.boxId, role };
    }
    async unpairBox(uid, boxId) {
        const box = await this.boxRepo.getById(boxId);
        if (!box)
            throw new error_handler_middleware_1.AppError(404, 'box_not_found', 'Box not found');
        let roleToUnpair = null;
        if (box.pairingInfo.senderUid === uid)
            roleToUnpair = 'sender';
        if (box.pairingInfo.receiverUid === uid)
            roleToUnpair = 'receiver';
        if (!roleToUnpair) {
            throw new error_handler_middleware_1.AppError(403, 'unauthorized', 'You are not paired to this box');
        }
        // Update Box
        const updateData = roleToUnpair === 'sender' ? { 'pairingInfo/senderUid': null } : { 'pairingInfo/receiverUid': null };
        await this.boxRepo.update(boxId, updateData);
        // Update User
        await this.userRepo.unlinkBox(uid, boxId);
        return roleToUnpair;
    }
    async getBoxDetails(uid, boxId) {
        const box = await this.boxRepo.getById(boxId);
        if (!box)
            throw new error_handler_middleware_1.AppError(404, 'box_not_found', 'Box not found');
        if (box.pairingInfo.senderUid !== uid && box.pairingInfo.receiverUid !== uid) {
            throw new error_handler_middleware_1.AppError(403, 'unauthorized', 'You are not paired to this box');
        }
        return box;
    }
    async updateWifi(uid, boxId, ssid, password) {
        await this.getBoxDetails(uid, boxId); // Validates ownership implicitly
        await this.boxRepo.update(boxId, {
            wifiConfig: {
                ssid,
                password,
                status: 'pending_apply'
            }
        });
        // Notify ESP32 via polling cache
        await this.boxRepo.updatePollingCache(boxId, { hasNewWifiConfig: true });
    }
}
exports.BoxService = BoxService;
//# sourceMappingURL=box.service.js.map