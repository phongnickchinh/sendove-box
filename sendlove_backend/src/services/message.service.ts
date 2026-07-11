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

    // Map loại file → đường dẫn Storage, content type, giới hạn dung lượng
    const typeMap: Record<string, { path: string; contentType: string; maxSize: number }> = {
      bin:            { path: `${basePath}/video.bin`,  contentType: 'application/octet-stream', maxSize: 15 * 1024 * 1024 }, // 15MB
      voice:          { path: `${basePath}/voice.wav`,  contentType: 'audio/wav',                maxSize: 2 * 1024 * 1024 },  // 2MB
      original_video: { path: `${basePath}/original.mp4`, contentType: 'video/mp4',              maxSize: 50 * 1024 * 1024 }, // 50MB
      original_image: { path: `${basePath}/original.jpg`, contentType: 'image/jpeg',             maxSize: 10 * 1024 * 1024 }, // 10MB
      original_gif:   { path: `${basePath}/original.gif`, contentType: 'image/gif',              maxSize: 20 * 1024 * 1024 }, // 20MB
      bg_music:       { path: `${basePath}/bgmusic.mp3`, contentType: 'audio/mpeg',              maxSize: 5 * 1024 * 1024 },  // 5MB
      thumbnail:      { path: `${basePath}/thumb.jpg`,  contentType: 'image/jpeg',               maxSize: 1 * 1024 * 1024 },  // 1MB
    };

    const upload_urls: Record<string, any> = {};

    for (const type of requestedTypes) {
      const config = typeMap[type];
      if (config) {
        upload_urls[type] = await this.storageRepo.generateUploadPolicy(config.path, config.contentType, config.maxSize);
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
    type: 'video' | 'image' | 'gif' | 'voice' | 'text';
    text?: string;
    duration?: number;
    frame_count?: number;
    width?: number;
    height?: number;
    uploaded_files?: string[]; // Array of keys from typeMap (e.g. ['bin', 'voice', 'original_video'])
  }): Promise<Message> {
    const now = Date.now();
    const basePath = `media/${boxId}/${data.message_id}`;

    // Auto-construct URLs based on uploaded_files or type conventions
    const uploaded = data.uploaded_files || [];
    
    const message: Omit<Message, 'id'> = {
      sender_id: senderId,
      box_id: boxId,
      timestamp: now,
      created_at: now,
      updated_at: now,
      type: data.type,
      text: data.text,
      duration: data.duration,
      frame_count: data.frame_count,
      width: data.width,
      height: data.height,
      
      // Auto-build URLs based on what was uploaded
      ...(uploaded.includes('bin') && { bin_url: `${basePath}/video.bin` }),
      ...(uploaded.includes('voice') && { voice_url: `${basePath}/voice.wav` }),
      ...(uploaded.includes('thumbnail') && { thumbnail_url: `${basePath}/thumb.jpg` }),
      ...(uploaded.includes('bg_music') && { bg_music_url: `${basePath}/bgmusic.mp3` }),
      ...(uploaded.includes('original_video') && { video_url: `${basePath}/original.mp4` }),
      ...(uploaded.includes('original_image') && { image_url: `${basePath}/original.jpg` }),
      ...(uploaded.includes('original_gif') && { gif_url: `${basePath}/original.gif` }),
    };

    // Verify files exist in Storage and calculate total_size
    let totalSize = 0;
    for (const key of uploaded) {
      // Map key back to filename based on typeMap logic
      const fileMap: Record<string, string> = {
        bin: 'video.bin',
        voice: 'voice.wav',
        thumbnail: 'thumb.jpg',
        bg_music: 'bgmusic.mp3',
        original_video: 'original.mp4',
        original_image: 'original.jpg',
        original_gif: 'original.gif',
      };
      const fileName = fileMap[key];
      if (fileName) {
        const filePath = `${basePath}/${fileName}`;
        try {
          const exists = await this.storageRepo.fileExists(filePath);
          if (exists) {
            const metadata = await this.storageRepo.getFileMetadata(filePath);
            totalSize += parseInt(metadata.size || '0', 10);
          } else {
            console.warn(`[MessageService] File not found during confirm: ${filePath}`);
          }
        } catch (error) {
          console.error(`[MessageService] Failed to get metadata for ${filePath}`, error);
        }
      }
    }
    
    message.total_size = totalSize;

    // Firebase RTDB does not allow undefined values. Clean them up.
    Object.keys(message).forEach(key => {
      if ((message as any)[key] === undefined) {
        delete (message as any)[key];
      }
    });

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
