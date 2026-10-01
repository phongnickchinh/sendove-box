"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.MessageService = void 0;
const firebase_message_repository_1 = require("../repositories/firebase/firebase-message.repository");
const firebase_storage_repository_1 = require("../repositories/firebase/firebase-storage.repository");
const error_handler_middleware_1 = require("../middleware/error-handler.middleware");
/** Signed media read URLs for the web expire after 15 minutes (same as /device/poll). */
const MEDIA_URL_MINUTES = 15;
class MessageService {
    constructor(msgRepo = new firebase_message_repository_1.FirebaseMessageRepository(), storageRepo = new firebase_storage_repository_1.FirebaseStorageRepository()) {
        this.msgRepo = msgRepo;
        this.storageRepo = storageRepo;
    }
    /**
     * Step 1: the sender asks to send a message → the backend creates signed upload URLs,
     * only for the file types the sender requested (saves GCS API calls).
     */
    async initiateMessage(boxId, senderId, requestedTypes) {
        const messageId = `msg_${Date.now()}`;
        const basePath = `media/${boxId}/${messageId}`;
        // File type → Storage path, content type, size limit
        const typeMap = {
            // 25MB: SD-card boxes take video up to 60s × 15 fps of 240×240 JPEG (~10-25KB/frame).
            // The box-side cap (MAX_MEDIA_BYTES) may be lower — that is the firmware's concern.
            bin: { path: `${basePath}/video.bin`, contentType: 'application/octet-stream', maxSize: 25 * 1024 * 1024 }, // 25MB
            voice: { path: `${basePath}/voice.wav`, contentType: 'audio/wav', maxSize: 2 * 1024 * 1024 }, // 2MB
            original_video: { path: `${basePath}/original.mp4`, contentType: 'video/mp4', maxSize: 50 * 1024 * 1024 }, // 50MB
            original_image: { path: `${basePath}/original.jpg`, contentType: 'image/jpeg', maxSize: 10 * 1024 * 1024 }, // 10MB
            original_gif: { path: `${basePath}/original.gif`, contentType: 'image/gif', maxSize: 20 * 1024 * 1024 }, // 20MB
            bg_music: { path: `${basePath}/bgmusic.wav`, contentType: 'audio/wav', maxSize: 5 * 1024 * 1024 }, // 5MB
            thumbnail: { path: `${basePath}/thumb.jpg`, contentType: 'image/jpeg', maxSize: 1 * 1024 * 1024 }, // 1MB
        };
        const upload_urls = {};
        for (const type of requestedTypes) {
            const config = typeMap[type];
            if (config) {
                upload_urls[type] = await this.storageRepo.generateUploadPolicy(config.path, config.contentType, config.maxSize);
            }
        }
        return { message_id: messageId, upload_urls };
    }
    /**
     * Step 2: uploads done → confirm writes the record to RTDB.
     * Only now does the message actually exist in the database.
     */
    async confirmMessage(boxId, senderId, data) {
        const now = Date.now();
        const basePath = `media/${boxId}/${data.message_id}`;
        // Auto-construct URLs based on uploaded_files or type conventions
        const uploaded = data.uploaded_files || [];
        const message = {
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
            ...(uploaded.includes('bg_music') && { bg_music_url: `${basePath}/bgmusic.wav` }),
            ...(uploaded.includes('original_video') && { video_url: `${basePath}/original.mp4` }),
            ...(uploaded.includes('original_image') && { image_url: `${basePath}/original.jpg` }),
            ...(uploaded.includes('original_gif') && { gif_url: `${basePath}/original.gif` }),
        };
        // Verify files exist in Storage and calculate total_size
        let totalSize = 0;
        for (const key of uploaded) {
            // Map key back to filename based on typeMap logic
            const fileMap = {
                bin: 'video.bin',
                voice: 'voice.wav',
                thumbnail: 'thumb.jpg',
                bg_music: 'bgmusic.wav',
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
                    }
                    else {
                        console.warn(`[MessageService] File not found during confirm: ${filePath}`);
                    }
                }
                catch (error) {
                    console.error(`[MessageService] Failed to get metadata for ${filePath}`, error);
                }
            }
        }
        message.total_size = totalSize;
        // Firebase RTDB does not allow undefined values. Clean them up.
        Object.keys(message).forEach(key => {
            if (message[key] === undefined) {
                delete message[key];
            }
        });
        return this.msgRepo.createMessage(boxId, data.message_id, message);
    }
    /** List messages (history). */
    async getMessages(boxId, limit) {
        const messages = await this.msgRepo.listMessages(boxId, limit);
        // Thumbnails for the list rows: sign only the thumbnail (small, ≤ 1 MB), in parallel.
        // Each signature is one IAM signBlob call — at most `limit` (≤ 100) per request.
        // If signing fails that row falls back to its icon; the list still loads.
        return Promise.all(messages.map(async (msg) => {
            if (!msg.thumbnail_url)
                return msg;
            try {
                return { ...msg, thumbnail: await this.storageRepo.generateDownloadUrl(msg.thumbnail_url, MEDIA_URL_MINUTES) };
            }
            catch (error) {
                console.error(`[MessageService] Failed to sign ${msg.thumbnail_url}`, error);
                return msg;
            }
        }));
    }
    /**
     * One message's details, with signed read URLs so the web can replay it.
     *
     * The *_url fields in RTDB are raw storage paths (confirmMessage) the browser
     * can't open, since storage.rules blocks everyone but the box. Only files the
     * browser can play are signed — NOT bin_url (the box's own SLBX format).
     */
    async getMessageDetails(boxId, messageId) {
        const msg = await this.msgRepo.getMessage(boxId, messageId);
        if (!msg)
            throw new error_handler_middleware_1.AppError(404, 'message_not_found', 'Message not found');
        const sources = {
            video: msg.video_url,
            image: msg.image_url || msg.gif_url,
            thumbnail: msg.thumbnail_url,
            voice: msg.voice_url,
            bg_music: msg.bg_music_url,
        };
        const media = {};
        await Promise.all(Object.keys(sources).map(async (key) => {
            const path = sources[key];
            if (!path)
                return;
            try {
                media[key] = await this.storageRepo.generateDownloadUrl(path, MEDIA_URL_MINUTES);
            }
            catch (error) {
                // One bad file must not break the whole popup — the web hides just
                // the missing part.
                console.error(`[MessageService] Failed to sign ${path}`, error);
            }
        }));
        return { ...msg, media };
    }
}
exports.MessageService = MessageService;
//# sourceMappingURL=message.service.js.map