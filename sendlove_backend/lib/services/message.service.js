"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.MessageService = void 0;
const firebase_message_repository_1 = require("../repositories/firebase/firebase-message.repository");
const firebase_storage_repository_1 = require("../repositories/firebase/firebase-storage.repository");
const error_handler_middleware_1 = require("../middleware/error-handler.middleware");
const media_processing_service_1 = require("./media-processing.service");
class MessageService {
    constructor() {
        this.msgRepo = new firebase_message_repository_1.FirebaseMessageRepository();
        this.storageRepo = new firebase_storage_repository_1.FirebaseStorageRepository();
        this.mediaService = new media_processing_service_1.MediaProcessingService();
    }
    async createMessage(boxId, data) {
        const messageId = `msg_${Date.now()}`;
        // Create DB entry
        await this.msgRepo.createMessage(boxId, messageId, {
            type: data.type,
            status: 'awaiting_upload',
            createdAt: Date.now(),
            text: data.text,
            metadata: data.metadata,
        });
        const uploadURLs = [];
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
            }
            else if (data.type.includes('video')) {
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
        }
        else {
            // If no media (text, text_music), directly trigger processing
            // In real life, we might do this asynchronously via Pub/Sub or immediate background promise
            this.mediaService.processMessageMedia(boxId, messageId, data.type, []).catch(console.error);
        }
        return { messageId, uploadURLs };
    }
    async completeUpload(boxId, messageId, uploadedFields) {
        const msg = await this.msgRepo.getMessage(boxId, messageId);
        if (!msg)
            throw new error_handler_middleware_1.AppError(404, 'message_not_found', 'Message not found');
        if (msg.status !== 'awaiting_upload') {
            throw new error_handler_middleware_1.AppError(400, 'invalid_status', 'Message is not waiting for upload');
        }
        await this.msgRepo.updateMessage(boxId, messageId, { status: 'processing' });
        // Trigger media processing async
        this.mediaService.processMessageMedia(boxId, messageId, msg.type, uploadedFields).catch(console.error);
    }
    async getMessages(boxId, limit) {
        return this.msgRepo.listMessages(boxId, limit);
    }
    async getMessageDetails(boxId, messageId) {
        const msg = await this.msgRepo.getMessage(boxId, messageId);
        if (!msg)
            throw new error_handler_middleware_1.AppError(404, 'message_not_found', 'Message not found');
        return msg;
    }
}
exports.MessageService = MessageService;
//# sourceMappingURL=message.service.js.map