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
const firebase_storage_repository_1 = require("../repositories/firebase/firebase-storage.repository");
const error_handler_middleware_1 = require("../middleware/error-handler.middleware");
const crypto_1 = __importDefault(require("crypto"));
class DeviceService {
    constructor(boxRepo = new firebase_box_repository_1.FirebaseBoxRepository(), msgRepo = new firebase_message_repository_1.FirebaseMessageRepository(), alarmRepo = new firebase_alarm_repository_1.FirebaseAlarmRepository(), fwRepo = new firebase_firmware_repository_1.FirebaseFirmwareRepository(), otaRepo = new firebase_ota_repository_1.FirebaseOtaRepository(), storageRepo = new firebase_storage_repository_1.FirebaseStorageRepository()) {
        this.boxRepo = boxRepo;
        this.msgRepo = msgRepo;
        this.alarmRepo = alarmRepo;
        this.fwRepo = fwRepo;
        this.otaRepo = otaRepo;
        this.storageRepo = storageRepo;
    }
    /** First-time ESP32 registration. */
    async registerDevice(data) {
        const boxId = `box_${data.deviceId}`;
        const now = Date.now();
        // Pairing codes: 9 alphanumeric chars (A-Z, 0-9) → 36^9 ≈ 1e14 combinations
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
                config_flag: false,
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
    /** Periodic ESP32 poll: flags, messages newer than last_download_ts, alarms when flagged. */
    async poll(boxId, lastDownloadTs, availableSlots = 3) {
        const box = await this.boxRepo.getById(boxId);
        if (!box)
            throw new error_handler_middleware_1.AppError(404, 'box_not_found', 'Box not found');
        const response = {
            server_time: new Date().toISOString(),
            flags: box.flags,
        };
        // New messages (when the ESP32 sent last_download_ts)
        if (lastDownloadTs !== undefined) {
            const allMessages = await this.msgRepo.listMessages(boxId, 50);
            const newMessages = allMessages
                .filter(m => m.timestamp > lastDownloadTs)
                .sort((a, b) => a.timestamp - b.timestamp)
                .slice(0, availableSlots);
            if (newMessages.length > 0) {
                response.new_messages = await Promise.all(newMessages.map(async (m) => {
                    let signedBinUrl = undefined;
                    let signedVoiceUrl = undefined;
                    if (m.bin_url) {
                        signedBinUrl = await this.storageRepo.generateDownloadUrl(m.bin_url, 15);
                    }
                    if (m.voice_url) {
                        signedVoiceUrl = await this.storageRepo.generateDownloadUrl(m.voice_url, 15);
                    }
                    return {
                        id: m.id,
                        timestamp: m.timestamp,
                        type: m.type,
                        duration: m.duration,
                        frame_count: m.frame_count,
                        width: m.width,
                        height: m.height,
                        text: m.text,
                        bin_url: signedBinUrl,
                        voice_url: signedVoiceUrl,
                        total_size: m.total_size,
                    };
                }));
            }
        }
        // a_flag → return the new alarm list
        if (box.flags.a_flag) {
            response.alarm_list = await this.alarmRepo.listAlarms(boxId);
            // Clear the flag once the ESP32 has read it
            await this.boxRepo.updateFlags(boxId, { a_flag: false });
        }
        // ota_flag → return the OTA task
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
        // p_flag → return the new pairing info
        if (box.flags.p_flag) {
            response.pairing = box.pairing;
            await this.boxRepo.updateFlags(boxId, { p_flag: false });
        }
        // config_flag → return the new led_state/display_brightness/playback_volume
        if (box.flags.config_flag) {
            response.config = {
                led_state: box.config.led_state,
                display_brightness: box.config.display_brightness,
                playback_volume: box.config.playback_volume,
            };
            await this.boxRepo.updateFlags(boxId, { config_flag: false });
        }
        // theme_flag → standby layout + a signed background URL. (The current firmware
        // reads RTDB directly and doesn't call /device/poll.)
        if (box.flags.theme_flag) {
            const theme = box.config?.theme;
            if (theme) {
                response.theme = {
                    theme_name: theme.theme_name,
                    widgets: theme.widgets,
                    background_url: theme.background
                        ? await this.storageRepo.generateDownloadUrl(theme.background, 15)
                        : null,
                    updated_at: theme.updated_at,
                };
            }
            await this.boxRepo.updateFlags(boxId, { theme_flag: false });
        }
        return response;
    }
    /** ESP32 heartbeat (updates status). */
    async heartbeat(boxId, data) {
        await this.boxRepo.updateStatus(boxId, {
            online: true,
            last_seen: Date.now(),
            ...(data.battery !== undefined && { battery: data.battery }),
            ...(data.charging !== undefined && { charging: data.charging }),
            ...(data.fw_version !== undefined && { fw_version: data.fw_version }),
            ...(data.storage_type !== undefined && { storage_type: data.storage_type }),
        });
    }
    /** The ESP32 reports that OTA finished. */
    async ackOta(boxId, taskId, success, errorMessage) {
        await this.otaRepo.updateOtaStatus(taskId, success ? 'completed' : 'failed', { error_message: errorMessage });
        if (success) {
            await this.boxRepo.updateFlags(boxId, { ota_flag: false });
        }
    }
}
exports.DeviceService = DeviceService;
//# sourceMappingURL=device.service.js.map