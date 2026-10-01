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

  /** First-time ESP32 registration. */
  async registerDevice(data: {
    deviceId: string;
    mac_address?: string;
    fw_version: string;
  }): Promise<any> {
    const boxId = `box_${data.deviceId}`;
    const now = Date.now();

    // Pairing codes: 9 alphanumeric chars (A-Z, 0-9) → 36^9 ≈ 1e14 combinations
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

  /**
   * Periodic ESP32 poll: check flags, fetch new messages, and the alarm list when needed.
   * The ESP32 sends last_download_ts → the backend returns messages with a newer timestamp.
   */
  async poll(boxId: string, lastDownloadTs?: number, availableSlots: number = 3): Promise<any> {
    const box = await this.boxRepo.getById(boxId);
    if (!box) throw new AppError(404, 'box_not_found', 'Box not found');

    const response: any = {
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

    // theme_flag → return the standby layout + a signed URL for the background.
    // (The current firmware reads RTDB directly and doesn't call /device/poll —
    // this path is for when the firmware switches to polling.)
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
  async heartbeat(boxId: string, data: {
    battery?: number;
    charging?: boolean;
    fw_version?: string;
    storage_type?: 'sd' | 'nand';
  }): Promise<void> {
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
