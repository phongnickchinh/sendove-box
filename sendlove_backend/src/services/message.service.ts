import { FirebaseMessageRepository } from '../repositories/firebase/firebase-message.repository';
import { FirebaseStorageRepository } from '../repositories/firebase/firebase-storage.repository';
import { Message, MessageType } from '../types/message.types';
import { AppError } from '../middleware/error-handler.middleware';
import { MediaProcessingService } from './media-processing.service';

export class MessageService {
  private msgRepo: FirebaseMessageRepository;
  private storageRepo: FirebaseStorageRepository;
  private mediaService: MediaProcessingService;

  constructor() {
    this.msgRepo = new FirebaseMessageRepository();
    this.storageRepo = new FirebaseStorageRepository();
    this.mediaService = new MediaProcessingService();
  }

  async createMessage(boxId: string, data: { type: MessageType, text?: string, metadata?: any, mediaCount?: number }): Promise<{ messageId: string, uploadURLs: { field: string, url: string }[] }> {
    const messageId = `msg_${Date.now()}`;
    
    // Create DB entry
    await this.msgRepo.createMessage(boxId, messageId, {
      type: data.type,
      status: 'awaiting_upload',
      createdAt: Date.now(),
      text: data.text,
      metadata: data.metadata,
    });

    const uploadURLs: { field: string, url: string }[] = [];

    // Generate signed URLs if media is expected
    if (data.type.includes('photo') || data.type.includes('video') || data.type.includes('voice')) {
      const count = data.mediaCount || 1;
      
      if (data.type.includes('photo')) {
        for (let i = 0; i < count; i++) {
          const field = `photo_${i}`;
          const path = `temp/${boxId}/${messageId}/${field}`;
          const url = await this.storageRepo.generateUploadUrl(path, 'image/jpeg'); // or image/*
          uploadURLs.push({ field, url });
        }
      } else if (data.type.includes('video')) {
        const field = `video`;
        const path = `temp/${boxId}/${messageId}/${field}`;
        const url = await this.storageRepo.generateUploadUrl(path, 'video/mp4');
        uploadURLs.push({ field, url });
      }

      if (data.type.includes('voice')) {
        const field = `voice`;
        const path = `temp/${boxId}/${messageId}/${field}`;
        const url = await this.storageRepo.generateUploadUrl(path, 'audio/webm'); // Web recording format
        uploadURLs.push({ field, url });
      }
    } else {
      // If no media (text, text_music), directly trigger processing
      // In real life, we might do this asynchronously via Pub/Sub or immediate background promise
      this.mediaService.processMessageMedia(boxId, messageId, data.type, []).catch(console.error);
    }

    return { messageId, uploadURLs };
  }

  async completeUpload(boxId: string, messageId: string, uploadedFields: string[]): Promise<void> {
    const msg = await this.msgRepo.getMessage(boxId, messageId);
    if (!msg) throw new AppError(404, 'message_not_found', 'Message not found');

    if (msg.status !== 'awaiting_upload') {
      throw new AppError(400, 'invalid_status', 'Message is not waiting for upload');
    }

    await this.msgRepo.updateMessage(boxId, messageId, { status: 'processing' });

    // Trigger media processing async
    this.mediaService.processMessageMedia(boxId, messageId, msg.type, uploadedFields).catch(console.error);
  }

  async getMessages(boxId: string, limit?: number): Promise<Message[]> {
    return this.msgRepo.listMessages(boxId, limit);
  }

  async getMessageDetails(boxId: string, messageId: string): Promise<Message> {
    const msg = await this.msgRepo.getMessage(boxId, messageId);
    if (!msg) throw new AppError(404, 'message_not_found', 'Message not found');
    return msg;
  }
}
