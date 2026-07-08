import { IMessageRepository } from '../repositories/interfaces/message.repository.interface';
import { IStorageRepository } from '../repositories/interfaces/storage.repository.interface';
import { FirebaseMessageRepository } from '../repositories/firebase/firebase-message.repository';
import { FirebaseStorageRepository } from '../repositories/firebase/firebase-storage.repository';
import { Message } from '../types/message.types';
import { AppError } from '../middleware/error-handler.middleware';

export class MessageService {
  constructor(
    private msgRepo: IMessageRepository = new FirebaseMessageRepository(),
    private storageRepo: IStorageRepository = new FirebaseStorageRepository()
  ) {}

  /**
   * Bước 1: Sender yêu cầu gửi tin nhắn → backend tạo signed upload URLs.
   * Chỉ tạo URL cho các loại file mà sender yêu cầu (tiết kiệm GCS API calls).
   */
  async initiateMessage(boxId: string, senderId: string, requestedTypes: string[]): Promise<{
    message_id: string;
    upload_urls: Record<string, string>;
  }> {
    const messageId = `msg_${Date.now()}`;
    const basePath = `media/${boxId}/${messageId}`;

    // Map loại file → đường dẫn Storage + content type
    const typeMap: Record<string, { path: string; contentType: string }> = {
      video: { path: `${basePath}/video.bin`, contentType: 'application/octet-stream' },
      voice: { path: `${basePath}/voice.wav`, contentType: 'audio/wav' },
      gif: { path: `${basePath}/animation.gif`, contentType: 'image/gif' },
      bg_music: { path: `${basePath}/bgmusic.mp3`, contentType: 'audio/mpeg' },
      image: { path: `${basePath}/photo.jpg`, contentType: 'image/jpeg' },
    };

    const upload_urls: Record<string, string> = {};

    for (const type of requestedTypes) {
      const config = typeMap[type];
      if (config) {
        upload_urls[type] = await this.storageRepo.generateUploadUrl(config.path, config.contentType);
      }
    }

    return { message_id: messageId, upload_urls };
  }

  /**
   * Bước 2: Sender upload xong → gọi confirm để ghi record vào RTDB.
   * Chỉ lúc này message mới thực sự "tồn tại" trên database.
   */
  async confirmMessage(boxId: string, senderId: string, data: {
    message_id: string;
    text?: string;
    bin_url?: string;
    voice_url?: string;
    gif_url?: string;
    bg_music_url?: string;
    image_url?: string;
    total_size?: number;
    thumbnail_url?: string;
  }): Promise<Message> {
    const now = Date.now();

    const message: Omit<Message, 'id'> = {
      sender_id: senderId,
      box_id: boxId,
      timestamp: now,
      created_at: now,
      updated_at: now,

      text: data.text,
      bin_url: data.bin_url,
      voice_url: data.voice_url,
      gif_url: data.gif_url,
      bg_music_url: data.bg_music_url,
      image_url: data.image_url,
      total_size: data.total_size,
      thumbnail_url: data.thumbnail_url,
    };

    return this.msgRepo.createMessage(boxId, data.message_id, message);
  }

  /**
   * Lấy danh sách tin nhắn (cho Receiver xem lịch sử)
   */
  async getMessages(boxId: string, limit?: number): Promise<Message[]> {
    return this.msgRepo.listMessages(boxId, limit);
  }

  /**
   * Lấy chi tiết 1 tin nhắn
   */
  async getMessageDetails(boxId: string, messageId: string): Promise<Message> {
    const msg = await this.msgRepo.getMessage(boxId, messageId);
    if (!msg) throw new AppError(404, 'message_not_found', 'Message not found');
    return msg;
  }
}
