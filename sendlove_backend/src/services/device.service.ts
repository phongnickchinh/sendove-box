import { FirebaseBoxRepository } from '../repositories/firebase/firebase-box.repository';
import { FirebaseMessageRepository } from '../repositories/firebase/firebase-message.repository';
import { FirebaseAlarmRepository } from '../repositories/firebase/firebase-alarm.repository';
import { FirebaseFirmwareRepository } from '../repositories/firebase/firebase-firmware.repository';
import { FirebaseOtaRepository } from '../repositories/firebase/firebase-ota.repository';
import { AppError } from '../middleware/error-handler.middleware';
import crypto from 'crypto';

export class DeviceService {
  private boxRepo: FirebaseBoxRepository;
  private msgRepo: FirebaseMessageRepository;
  private alarmRepo: FirebaseAlarmRepository;
  private fwRepo: FirebaseFirmwareRepository;
  private otaRepo: FirebaseOtaRepository;

  constructor() {
    this.boxRepo = new FirebaseBoxRepository();
    this.msgRepo = new FirebaseMessageRepository();
    this.alarmRepo = new FirebaseAlarmRepository();
    this.fwRepo = new FirebaseFirmwareRepository();
    this.otaRepo = new FirebaseOtaRepository();
  }

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

    // Sinh pairing codes
    const rcode = `R${crypto.randomBytes(3).toString('hex').toUpperCase()}`;
    const scode = `S${crypto.randomBytes(3).toString('hex').toUpperCase()}`;
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
  async poll(boxId: string, lastDownloadTs?: number): Promise<any> {
    const box = await this.boxRepo.getById(boxId);
    if (!box) throw new AppError(404, 'box_not_found', 'Box not found');

    const response: any = {
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
