"use strict";
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
exports.DeviceService = void 0;
const firebase_box_repository_1 = require("../repositories/firebase/firebase-box.repository");
const firebase_message_repository_1 = require("../repositories/firebase/firebase-message.repository");
const firebase_alarm_repository_1 = require("../repositories/firebase/firebase-alarm.repository");
const firebase_storage_repository_1 = require("../repositories/firebase/firebase-storage.repository");
const error_handler_middleware_1 = require("../middleware/error-handler.middleware");
const crypto_1 = __importDefault(require("crypto"));
const config_1 = require("../config");
class DeviceService {
    constructor() {
        this.boxRepo = new firebase_box_repository_1.FirebaseBoxRepository();
        this.msgRepo = new firebase_message_repository_1.FirebaseMessageRepository();
        this.alarmRepo = new firebase_alarm_repository_1.FirebaseAlarmRepository();
        this.storageRepo = new firebase_storage_repository_1.FirebaseStorageRepository();
    }
    async registerDevice(data) {
        const boxId = `box_${data.deviceId}`;
        const deviceSecret = crypto_1.default.randomBytes(32).toString('hex');
        await this.boxRepo.create(boxId, {
            boxId,
            macAddress: data.macAddress,
            deviceSecret,
            firmwareVersion: data.firmwareVersion,
            isOnline: true,
            lastSeen: Date.now(),
            pairingInfo: {
                senderCode: data.senderCode,
                receiverCode: data.receiverCode
            }
        });
        // Initialize polling cache
        await this.boxRepo.updatePollingCache(boxId, {
            hasNewMessage: false,
            latestMessageId: null,
            hasNewAlarms: false,
            hasNewWifiConfig: false,
            pollIntervalSeconds: 30
        });
        return {
            boxId,
            deviceSecret,
            pollIntervalSeconds: 30,
            serverTime: new Date().toISOString()
        };
    }
    async poll(boxId) {
        const cache = await this.boxRepo.getPollingCache(boxId);
        if (!cache)
            throw new error_handler_middleware_1.AppError(404, 'box_not_found', 'Box not found');
        const response = {
            serverTime: new Date().toISOString(),
            pollIntervalSeconds: cache.pollIntervalSeconds,
            alarmsUpdated: cache.hasNewAlarms,
            wifiConfig: null,
        };
        // If new message
        if (cache.hasNewMessage && cache.latestMessageId) {
            const msg = await this.msgRepo.getMessage(boxId, cache.latestMessageId);
            if (msg && msg.status === 'ready' && msg.storagePaths) {
                response.newMessage = {
                    messageId: msg.messageId,
                    type: msg.type,
                    metadata: msg.metadata,
                    files: Object.keys(msg.storagePaths).map(key => ({
                        fileType: key,
                        // Assuming download API will handle piping the storage file
                        url: `/api/device/download/${msg.messageId}/${key}`
                    }))
                };
            }
        }
        // If alarms updated
        if (cache.hasNewAlarms) {
            response.alarms = await this.alarmRepo.listAlarms(boxId);
            // Reset flag
            await this.boxRepo.updatePollingCache(boxId, { hasNewAlarms: false });
        }
        // If wifi config updated
        if (cache.hasNewWifiConfig) {
            const box = await this.boxRepo.getById(boxId);
            if (box && box.wifiConfig && box.wifiConfig.status === 'pending_apply') {
                response.wifiConfig = {
                    ssid: box.wifiConfig.ssid,
                    password: box.wifiConfig.password
                };
            }
        }
        return response;
    }
    async heartbeat(boxId, data) {
        await this.boxRepo.update(boxId, {
            isOnline: true,
            lastSeen: Date.now(),
            // could store telemetry (freeHeap, rssi) in a separate node if needed
        });
    }
    async ackMessage(boxId, messageId, status) {
        const msg = await this.msgRepo.getMessage(boxId, messageId);
        if (!msg)
            throw new error_handler_middleware_1.AppError(404, 'message_not_found', 'Message not found');
        await this.msgRepo.updateMessage(boxId, messageId, { status: 'delivered' });
        // Clear polling cache flag
        const cache = await this.boxRepo.getPollingCache(boxId);
        if (cache?.latestMessageId === messageId) {
            await this.boxRepo.updatePollingCache(boxId, { hasNewMessage: false, latestMessageId: null });
        }
        // Optionally auto-delete binary files to save space
        if (config_1.config.storage.autoDeleteAfterDownload && msg.storagePaths) {
            for (const path of Object.values(msg.storagePaths)) {
                await this.storageRepo.deleteFile(path).catch(e => console.error(`Failed to delete ${path}`, e));
            }
        }
    }
}
exports.DeviceService = DeviceService;
//# sourceMappingURL=device.service.js.map