"use strict";
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
exports.DeviceService = void 0;
const firebase_box_repository_1 = require("../repositories/firebase/firebase-box.repository");
const firebase_message_repository_1 = require("../repositories/firebase/firebase-message.repository");
const firebase_alarm_repository_1 = require("../repositories/firebase/firebase-alarm.repository");
const firebase_firmware_repository_1 = require("../repositories/firebase/firebase-firmware.repository");
const firebase_ota_repository_1 = require("../repositories/firebase/firebase-ota.repository");
const error_handler_middleware_1 = require("../middleware/error-handler.middleware");
const crypto_1 = __importDefault(require("crypto"));
class DeviceService {
    constructor(boxRepo = new firebase_box_repository_1.FirebaseBoxRepository(), msgRepo = new firebase_message_repository_1.FirebaseMessageRepository(), alarmRepo = new firebase_alarm_repository_1.FirebaseAlarmRepository(), fwRepo = new firebase_firmware_repository_1.FirebaseFirmwareRepository(), otaRepo = new firebase_ota_repository_1.FirebaseOtaRepository()) {
        this.boxRepo = boxRepo;
        this.msgRepo = msgRepo;
        this.alarmRepo = alarmRepo;
        this.fwRepo = fwRepo;
        this.otaRepo = otaRepo;
    }
    /**
     * ESP32 đăng ký lần đầu
     */
    async registerDevice(data) {
        const boxId = `box_${data.deviceId}`;
        const now = Date.now();
        // Sinh pairing codes: 9 ký tự alphanumeric (A-Z, 0-9) → 36^9 ≈ 101 nghìn tỷ combinations
        const alphanumChars = 'ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789';
        const generateCode = (length) => {
            const bytes = crypto_1.default.randomBytes(length);
            return Array.from(bytes).map(b => alphanumChars[b % alphanumChars.length]).join('');
        };
        const rcode = `R${generateCode(9)}`;
        const scode = `S${generateCode(9)}`;
        const deviceSecret = crypto_1.default.randomBytes(16).toString('hex');
        await this.boxRepo.create(boxId, {
            id: boxId,
            device_secret: deviceSecret,
            created_at: now,
            updated_at: now,
            code: {
                rcode,
                scode,
                rcode_created_at: now,
                scode_created_at: now,
            },
            pairing: {
                sender_id: null,
                receiver_id: null,
                sender_paired_time: null,
                receiver_paired_time: null,
            },
            config: {
                alarm_list: {},
            },
            flags: {
                a_flag: false,
                ota_flag: false,
                p_flag: false,
            },
            status: {
                online: true,
                charging: false,
                battery: 100,
                fw_version: data.fw_version,
                last_seen: now,
            },
        });
        return {
            box_id: boxId,
            device_secret: deviceSecret,
            rcode,
            scode,
            server_time: new Date().toISOString(),
        };
    }
    /**
     * ESP32 poll định kỳ: kiểm tra flags, lấy messages mới, lấy alarm list nếu cần.
     * ESP32 gửi last_download_ts → backend trả messages có timestamp > last_download_ts.
     */
    async poll(boxId, lastDownloadTs) {
        const box = await this.boxRepo.getById(boxId);
        if (!box)
            throw new error_handler_middleware_1.AppError(404, 'box_not_found', 'Box not found');
        const response = {
            server_time: new Date().toISOString(),
            flags: box.flags,
        };
        // Nếu có tin nhắn mới (ESP32 gửi last_download_ts)
        if (lastDownloadTs !== undefined) {
            const allMessages = await this.msgRepo.listMessages(boxId, 20);
            const newMessages = allMessages.filter(m => m.timestamp > lastDownloadTs);
            if (newMessages.length > 0) {
                response.new_messages = newMessages.map(m => ({
                    id: m.id,
                    timestamp: m.timestamp,
                    text: m.text,
                    bin_url: m.bin_url,
                    voice_url: m.voice_url,
                    gif_url: m.gif_url,
                    bg_music_url: m.bg_music_url,
                    image_url: m.image_url,
                    total_size: m.total_size,
                }));
            }
        }
        // Nếu a_flag = true → trả alarm list mới
        if (box.flags.a_flag) {
            response.alarm_list = await this.alarmRepo.listAlarms(boxId);
            // Reset flag sau khi ESP32 đã đọc
            await this.boxRepo.updateFlags(boxId, { a_flag: false });
        }
        // Nếu ota_flag = true → trả thông tin OTA task
        if (box.flags.ota_flag) {
            const otaTask = await this.otaRepo.findPendingByBoxId(boxId);
            if (otaTask) {
                const fw = await this.fwRepo.getById(otaTask.fw_version);
                response.ota = {
                    task_id: otaTask.id,
                    fw_version: otaTask.fw_version,
                    storage_url: fw?.storage_url,
                    checksum: fw?.checksum,
                };
            }
        }
        // Nếu p_flag = true → trả pairing info mới
        if (box.flags.p_flag) {
            response.pairing = box.pairing;
            await this.boxRepo.updateFlags(boxId, { p_flag: false });
        }
        return response;
    }
    /**
     * ESP32 gửi heartbeat (cập nhật status)
     */
    async heartbeat(boxId, data) {
        await this.boxRepo.updateStatus(boxId, {
            online: true,
            last_seen: Date.now(),
            ...(data.battery !== undefined && { battery: data.battery }),
            ...(data.charging !== undefined && { charging: data.charging }),
            ...(data.fw_version !== undefined && { fw_version: data.fw_version }),
        });
    }
    /**
     * ESP32 báo OTA hoàn tất
     */
    async ackOta(boxId, taskId, success, errorMessage) {
        await this.otaRepo.updateOtaStatus(taskId, success ? 'completed' : 'failed', { error_message: errorMessage });
        if (success) {
            await this.boxRepo.updateFlags(boxId, { ota_flag: false });
        }
    }
}
exports.DeviceService = DeviceService;
//# sourceMappingURL=device.service.js.map