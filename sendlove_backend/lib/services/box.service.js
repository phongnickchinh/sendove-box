"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.BoxService = void 0;
const firebase_box_repository_1 = require("../repositories/firebase/firebase-box.repository");
const firebase_user_repository_1 = require("../repositories/firebase/firebase-user.repository");
const error_handler_middleware_1 = require("../middleware/error-handler.middleware");
class BoxService {
    constructor(boxRepo = new firebase_box_repository_1.FirebaseBoxRepository(), userRepo = new firebase_user_repository_1.FirebaseUserRepository()) {
        this.boxRepo = boxRepo;
        this.userRepo = userRepo;
    }
    /**
     * Pairing: User nhập mã pairing code → liên kết user với box.
     * scode bắt đầu bằng 'S', rcode bắt đầu bằng 'R'.
     */
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
        // Kiểm tra slot đã bị chiếm chưa
        if (isSender && pairing.sender_id) {
            throw new error_handler_middleware_1.AppError(400, 'slot_full', 'Sender slot is already taken');
        }
        if (!isSender && pairing.receiver_id) {
            throw new error_handler_middleware_1.AppError(400, 'slot_full', 'Receiver slot is already taken');
        }
        // Kiểm tra user không thể vừa sender vừa receiver trên cùng 1 box
        const isAlreadyReceiver = isSender && (pairing.receiver_id === uid);
        const isAlreadySender = !isSender && (pairing.sender_id === uid);
        if (isAlreadyReceiver || isAlreadySender) {
            throw new error_handler_middleware_1.AppError(400, 'conflict_role', 'You cannot be both sender and receiver for the same box');
        }
        const now = Date.now();
        // Cập nhật Box pairing
        const pairingUpdate = isSender
            ? { 'pairing/sender_id': uid, 'pairing/sender_paired_time': now }
            : { 'pairing/receiver_id': uid, 'pairing/receiver_paired_time': now };
        await this.boxRepo.update(box.id, { ...pairingUpdate, updated_at: now });
        // Set p_flag để ESP32 biết có thay đổi pairing
        await this.boxRepo.updateFlags(box.id, { p_flag: true });
        // Cập nhật User boxes_list
        await this.userRepo.linkBox(uid, box.id, { role, box_name: boxName });
        return { boxId: box.id, role };
    }
    /**
     * Unpair: Ngắt kết nối user khỏi box.
     */
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
        // Cập nhật Box
        const pairingUpdate = roleToUnpair === 'sender'
            ? { 'pairing/sender_id': null, 'pairing/sender_paired_time': null }
            : { 'pairing/receiver_id': null, 'pairing/receiver_paired_time': null };
        await this.boxRepo.update(boxId, { ...pairingUpdate, updated_at: now });
        // Set p_flag
        await this.boxRepo.updateFlags(boxId, { p_flag: true });
        // Cập nhật User
        await this.userRepo.unlinkBox(uid, boxId);
        return roleToUnpair;
    }
    /**
     * Xem chi tiết box (chỉ user đã pair mới xem được).
     * Lọc bỏ device_secret và wifi password trước khi trả về.
     */
    async getBoxDetails(uid, boxId) {
        const box = await this.boxRepo.getById(boxId);
        if (!box)
            throw new error_handler_middleware_1.AppError(404, 'box_not_found', 'Box not found');
        const pairing = box.pairing || {};
        if (pairing.sender_id !== uid && pairing.receiver_id !== uid) {
            throw new error_handler_middleware_1.AppError(403, 'unauthorized', 'You are not paired to this box');
        }
        // Sanitize: loại bỏ sensitive fields trước khi trả về API
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
     * Cập nhật cấu hình WiFi cho box
     */
    async updateWifi(uid, boxId, ssid, pwd) {
        await this.getBoxDetails(uid, boxId); // Validates ownership
        const now = Date.now();
        await this.boxRepo.update(boxId, {
            'config/wifi_config': { ssid, pwd: pwd || '' },
            updated_at: now,
        });
    }
}
exports.BoxService = BoxService;
//# sourceMappingURL=box.service.js.map