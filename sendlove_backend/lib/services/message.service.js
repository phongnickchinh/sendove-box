"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.MessageService = void 0;
const firebase_message_repository_1 = require("../repositories/firebase/firebase-message.repository");
const firebase_storage_repository_1 = require("../repositories/firebase/firebase-storage.repository");
const error_handler_middleware_1 = require("../middleware/error-handler.middleware");
class MessageService {
    constructor(msgRepo = new firebase_message_repository_1.FirebaseMessageRepository(), storageRepo = new firebase_storage_repository_1.FirebaseStorageRepository()) {
        this.msgRepo = msgRepo;
        this.storageRepo = storageRepo;
    }
    /**
     * Bước 1: Sender yêu cầu gửi tin nhắn → backend tạo signed upload URLs.
     * Chỉ tạo URL cho các loại file mà sender yêu cầu (tiết kiệm GCS API calls).
     */
    async initiateMessage(boxId, senderId, requestedTypes) {
        const messageId = `msg_${Date.now()}`;
        const basePath = `media/${boxId}/${messageId}`;
        // Map loại file → đường dẫn Storage + content type
        const typeMap = {
            video: { path: `${basePath}/video.bin`, contentType: 'application/octet-stream' },
            voice: { path: `${basePath}/voice.wav`, contentType: 'audio/wav' },
            gif: { path: `${basePath}/animation.gif`, contentType: 'image/gif' },
            bg_music: { path: `${basePath}/bgmusic.mp3`, contentType: 'audio/mpeg' },
            image: { path: `${basePath}/photo.jpg`, contentType: 'image/jpeg' },
        };
        const upload_urls = {};
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
    async confirmMessage(boxId, senderId, data) {
        const now = Date.now();
        const message = {
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
    async getMessages(boxId, limit) {
        return this.msgRepo.listMessages(boxId, limit);
    }
    /**
     * Lấy chi tiết 1 tin nhắn
     */
    async getMessageDetails(boxId, messageId) {
        const msg = await this.msgRepo.getMessage(boxId, messageId);
        if (!msg)
            throw new error_handler_middleware_1.AppError(404, 'message_not_found', 'Message not found');
        return msg;
    }
}
exports.MessageService = MessageService;
//# sourceMappingURL=message.service.js.map