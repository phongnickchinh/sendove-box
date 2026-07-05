import { FirebaseBoxRepository } from '../repositories/firebase/firebase-box.repository';
import { FirebaseMessageRepository } from '../repositories/firebase/firebase-message.repository';
import { FirebaseAlarmRepository } from '../repositories/firebase/firebase-alarm.repository';
import { FirebaseStorageRepository } from '../repositories/firebase/firebase-storage.repository';
import { AppError } from '../middleware/error-handler.middleware';
import crypto from 'crypto';
import { config } from '../config';

export class DeviceService {
  private boxRepo: FirebaseBoxRepository;
  private msgRepo: FirebaseMessageRepository;
  private alarmRepo: FirebaseAlarmRepository;
  private storageRepo: FirebaseStorageRepository;

  constructor() {
    this.boxRepo = new FirebaseBoxRepository();
    this.msgRepo = new FirebaseMessageRepository();
    this.alarmRepo = new FirebaseAlarmRepository();
    this.storageRepo = new FirebaseStorageRepository();
  }

  async registerDevice(data: { deviceId: string, macAddress: string, firmwareVersion: string, senderCode: string, receiverCode: string }): Promise<any> {
    const boxId = `box_${data.deviceId}`;
    const deviceSecret = crypto.randomBytes(32).toString('hex');

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

  async poll(boxId: string): Promise<any> {
    const cache = await this.boxRepo.getPollingCache(boxId);
    if (!cache) throw new AppError(404, 'box_not_found', 'Box not found');

    const response: any = {
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

  async heartbeat(boxId: string, data: any): Promise<void> {
    await this.boxRepo.update(boxId, {
      isOnline: true,
      lastSeen: Date.now(),
      // could store telemetry (freeHeap, rssi) in a separate node if needed
    });
  }

  async ackMessage(boxId: string, messageId: string, status: string): Promise<void> {
    const msg = await this.msgRepo.getMessage(boxId, messageId);
    if (!msg) throw new AppError(404, 'message_not_found', 'Message not found');

    await this.msgRepo.updateMessage(boxId, messageId, { status: 'delivered' });
    
    // Clear polling cache flag
    const cache = await this.boxRepo.getPollingCache(boxId);
    if (cache?.latestMessageId === messageId) {
      await this.boxRepo.updatePollingCache(boxId, { hasNewMessage: false, latestMessageId: null });
    }

    // Optionally auto-delete binary files to save space
    if (config.storage.autoDeleteAfterDownload && msg.storagePaths) {
      for (const path of Object.values(msg.storagePaths)) {
        await this.storageRepo.deleteFile(path).catch(e => console.error(`Failed to delete ${path}`, e));
      }
    }
  }
}
