import { FirebaseMessageRepository } from '../repositories/firebase/firebase-message.repository';
import { FirebaseStorageRepository } from '../repositories/firebase/firebase-storage.repository';
import { Message } from '../types/message.types';
import { AppError } from '../middleware/error-handler.middleware';

export class MessageService {
  private msgRepo: FirebaseMessageRepository;
  private storageRepo: FirebaseStorageRepository;

  constructor() {
    this.msgRepo = new FirebaseMessageRepository();
    this.storageRepo = new FirebaseStorageRepository();
  }

  /**
   * Bước 1: Sender yêu cầu gửi tin nhắn → backend tạo signed upload URLs.
   * Sender chưa biết URL cuối cùng, chỉ nhận URL tạm để upload file.
   */
  async initiateMessage(boxId: string, senderId: string): Promise<{
    message_id: string;
    upload_urls: { bin?: string; voice?: string; gif?: string; bg_music?: string; image?: string };
  }> {
    const messageId = `msg_${Date.now()}`;
    const basePath = `media/${boxId}/${messageId}`;

    const upload_urls: Record<string, string> = {};

    // Tạo signed upload URL cho mỗi loại file có thể có
    upload_urls.bin = await this.storageRepo.generateUploadUrl(
      `${basePath}/video.bin`, 'application/octet-stream'
    );
    upload_urls.voice = await this.storageRepo.generateUploadUrl(
      `${basePath}/voice.wav`, 'audio/wav'
    );
    upload_urls.gif = await this.storageRepo.generateUploadUrl(
      `${basePath}/animation.gif`, 'image/gif'
    );
    upload_urls.bg_music = await this.storageRepo.generateUploadUrl(
      `${basePath}/bgmusic.mp3`, 'audio/mpeg'
    );
    upload_urls.image = await this.storageRepo.generateUploadUrl(
      `${basePath}/photo.jpg`, 'image/jpeg'
    );

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
