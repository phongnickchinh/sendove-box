import { IBoxRepository } from '../repositories/interfaces/box.repository.interface';
import { IMessageRepository } from '../repositories/interfaces/message.repository.interface';
import { IAlarmRepository } from '../repositories/interfaces/alarm.repository.interface';
import { IFirmwareRepository } from '../repositories/interfaces/firmware.repository.interface';
import { IOtaRepository } from '../repositories/interfaces/ota.repository.interface';
import { IStorageRepository } from '../repositories/interfaces/storage.repository.interface';
import { FirebaseBoxRepository } from '../repositories/firebase/firebase-box.repository';
import { FirebaseMessageRepository } from '../repositories/firebase/firebase-message.repository';
import { FirebaseAlarmRepository } from '../repositories/firebase/firebase-alarm.repository';
import { FirebaseFirmwareRepository } from '../repositories/firebase/firebase-firmware.repository';
import { FirebaseOtaRepository } from '../repositories/firebase/firebase-ota.repository';
import { FirebaseStorageRepository } from '../repositories/firebase/firebase-storage.repository';
import { AppError } from '../middleware/error-handler.middleware';
import crypto from 'crypto';

export class DeviceService {
  constructor(
    private boxRepo: IBoxRepository = new FirebaseBoxRepository(),
    private msgRepo: IMessageRepository = new FirebaseMessageRepository(),
    private alarmRepo: IAlarmRepository = new FirebaseAlarmRepository(),
    private fwRepo: IFirmwareRepository = new FirebaseFirmwareRepository(),
    private otaRepo: IOtaRepository = new FirebaseOtaRepository(),
    private storageRepo: IStorageRepository = new FirebaseStorageRepository()
  ) {}

  /**
   * ESP32 đăng ký lần đầu
   */
  async registerDevice(data: {
    deviceId: string;
    mac_address?: string;
    fw_version: string;
  }): Promise<any> {
    const boxId = `box_${data.deviceId}`;
    const now = Date.now();

    // Sinh pairing codes: 9 ký tự alphanumeric (A-Z, 0-9) → 36^9 ≈ 101 nghìn tỷ combinations
    const alphanumChars = 'ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789';
    const generateCode = (length: number): string => {
      const bytes = crypto.randomBytes(length);
      return Array.from(bytes).map(b => alphanumChars[b % alphanumChars.length]).join('');
    };

    const rcode = `R${generateCode(9)}`;
    const scode = `S${generateCode(9)}`;
    const deviceSecret = crypto.randomBytes(16).toString('hex');

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
  async poll(boxId: string, lastDownloadTs?: number, availableSlots: number = 3): Promise<any> {
    const box = await this.boxRepo.getById(boxId);
    if (!box) throw new AppError(404, 'box_not_found', 'Box not found');

    const response: any = {
      server_time: new Date().toISOString(),
      flags: box.flags,
    };

    // Nếu có tin nhắn mới (ESP32 gửi last_download_ts)
    if (lastDownloadTs !== undefined) {
      const allMessages = await this.msgRepo.listMessages(boxId, 50);
      
      const newMessages = allMessages
        .filter(m => m.timestamp > lastDownloadTs)
        .sort((a, b) => a.timestamp - b.timestamp)
        .slice(0, availableSlots);

      if (newMessages.length > 0) {
        response.new_messages = await Promise.all(newMessages.map(async m => {
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
  async heartbeat(boxId: string, data: {
    battery?: number;
    charging?: boolean;
    fw_version?: string;
  }): Promise<void> {
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
  async ackOta(boxId: string, taskId: string, success: boolean, errorMessage?: string): Promise<void> {
    await this.otaRepo.updateOtaStatus(
      taskId,
      success ? 'completed' : 'failed',
      { error_message: errorMessage }
    );

    if (success) {
      await this.boxRepo.updateFlags(boxId, { ota_flag: false });
    }
  }
}
