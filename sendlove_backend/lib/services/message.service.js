"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.MessageService = void 0;
const firebase_message_repository_1 = require("../repositories/firebase/firebase-message.repository");
const firebase_storage_repository_1 = require("../repositories/firebase/firebase-storage.repository");
const error_handler_middleware_1 = require("../middleware/error-handler.middleware");
class MessageService {
    constructor() {
        this.msgRepo = new firebase_message_repository_1.FirebaseMessageRepository();
        this.storageRepo = new firebase_storage_repository_1.FirebaseStorageRepository();
    }
    /**
     * Bước 1: Sender yêu cầu gửi tin nhắn → backend tạo signed upload URLs.
     * Sender chưa biết URL cuối cùng, chỉ nhận URL tạm để upload file.
     */
    async initiateMessage(boxId, senderId) {
        const messageId = `msg_${Date.now()}`;
        const basePath = `media/${boxId}/${messageId}`;
        const upload_urls = {};
        // Tạo signed upload URL cho mỗi loại file có thể có
        upload_urls.bin = await this.storageRepo.generateUploadUrl(`${basePath}/video.bin`, 'application/octet-stream');
        upload_urls.voice = await this.storageRepo.generateUploadUrl(`${basePath}/voice.wav`, 'audio/wav');
        upload_urls.gif = await this.storageRepo.generateUploadUrl(`${basePath}/animation.gif`, 'image/gif');
        upload_urls.bg_music = await this.storageRepo.generateUploadUrl(`${basePath}/bgmusic.mp3`, 'audio/mpeg');
        upload_urls.image = await this.storageRepo.generateUploadUrl(`${basePath}/photo.jpg`, 'image/jpeg');
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