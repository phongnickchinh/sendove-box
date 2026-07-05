"use strict";
var __createBinding = (this && this.__createBinding) || (Object.create ? (function(o, m, k, k2) {
    if (k2 === undefined) k2 = k;
    var desc = Object.getOwnPropertyDescriptor(m, k);
    if (!desc || ("get" in desc ? !m.__esModule : desc.writable || desc.configurable)) {
      desc = { enumerable: true, get: function() { return m[k]; } };
    }
    Object.defineProperty(o, k2, desc);
}) : (function(o, m, k, k2) {
    if (k2 === undefined) k2 = k;
    o[k2] = m[k];
}));
var __setModuleDefault = (this && this.__setModuleDefault) || (Object.create ? (function(o, v) {
    Object.defineProperty(o, "default", { enumerable: true, value: v });
}) : function(o, v) {
    o["default"] = v;
});
var __importStar = (this && this.__importStar) || (function () {
    var ownKeys = function(o) {
        ownKeys = Object.getOwnPropertyNames || function (o) {
            var ar = [];
            for (var k in o) if (Object.prototype.hasOwnProperty.call(o, k)) ar[ar.length] = k;
            return ar;
        };
        return ownKeys(o);
    };
    return function (mod) {
        if (mod && mod.__esModule) return mod;
        var result = {};
        if (mod != null) for (var k = ownKeys(mod), i = 0; i < k.length; i++) if (k[i] !== "default") __createBinding(result, mod, k[i]);
        __setModuleDefault(result, mod);
        return result;
    };
})();
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
exports.MediaProcessingService = void 0;
const firebase_storage_repository_1 = require("../repositories/firebase/firebase-storage.repository");
const firebase_message_repository_1 = require("../repositories/firebase/firebase-message.repository");
const firebase_box_repository_1 = require("../repositories/firebase/firebase-box.repository");
const config_1 = require("../config");
const os = __importStar(require("os"));
const path = __importStar(require("path"));
const fs = __importStar(require("fs"));
const fluent_ffmpeg_1 = __importDefault(require("fluent-ffmpeg"));
const ffmpeg_1 = __importDefault(require("@ffmpeg-installer/ffmpeg"));
const sharp_1 = __importDefault(require("sharp"));
// Set ffmpeg path
fluent_ffmpeg_1.default.setFfmpegPath(ffmpeg_1.default.path);
class MediaProcessingService {
    constructor() {
        this.storageRepo = new firebase_storage_repository_1.FirebaseStorageRepository();
        this.msgRepo = new firebase_message_repository_1.FirebaseMessageRepository();
        this.boxRepo = new firebase_box_repository_1.FirebaseBoxRepository();
    }
    async processMessageMedia(boxId, messageId, type, uploadedFields) {
        const tempDir = path.join(os.tmpdir(), messageId);
        try {
            // Create temp dir
            if (!fs.existsSync(tempDir)) {
                fs.mkdirSync(tempDir, { recursive: true });
            }
            const storagePaths = {};
            // Handle photos
            if (type.includes('photo')) {
                for (const field of uploadedFields) {
                    if (field.startsWith('photo_')) {
                        const tempLocalPath = path.join(tempDir, `${field}_original`);
                        const outLocalPath = path.join(tempDir, `${field}.bin`);
                        // Download
                        await this.storageRepo.downloadToLocal(`temp/${boxId}/${messageId}/${field}`, tempLocalPath);
                        // Convert to RGB565 raw binary
                        await this.processImage(tempLocalPath, outLocalPath);
                        // Upload
                        const destPath = `boxes/${boxId}/${messageId}/${field}.bin`;
                        await this.storageRepo.uploadFromLocal(outLocalPath, destPath, 'application/octet-stream');
                        storagePaths[field] = destPath;
                    }
                }
            }
            // Handle video
            if (type.includes('video') && uploadedFields.includes('video')) {
                const tempLocalPath = path.join(tempDir, `video_original`);
                const outFramesDir = path.join(tempDir, `frames`);
                fs.mkdirSync(outFramesDir);
                await this.storageRepo.downloadToLocal(`temp/${boxId}/${messageId}/video`, tempLocalPath);
                // Extract frames
                const frameFiles = await this.extractVideoFrames(tempLocalPath, outFramesDir);
                // Convert frames to RGB565
                for (let i = 0; i < frameFiles.length; i++) {
                    const framePath = frameFiles[i];
                    const field = `frame_${i}`;
                    const outLocalPath = path.join(tempDir, `${field}.bin`);
                    await this.processImage(framePath, outLocalPath);
                    const destPath = `boxes/${boxId}/${messageId}/${field}.bin`;
                    await this.storageRepo.uploadFromLocal(outLocalPath, destPath, 'application/octet-stream');
                    storagePaths[field] = destPath;
                }
            }
            // Handle audio (voice or background music)
            let audioSourcePath = null;
            if (type.includes('voice') && uploadedFields.includes('voice')) {
                audioSourcePath = path.join(tempDir, 'voice_original');
                await this.storageRepo.downloadToLocal(`temp/${boxId}/${messageId}/voice`, audioSourcePath);
            }
            else if (type.includes('music')) {
                // TODO: Download background music from library
                // audioSourcePath = ...
            }
            if (audioSourcePath) {
                const outAudioPath = path.join(tempDir, 'audio.wav');
                await this.processAudio(audioSourcePath, outAudioPath);
                const destPath = `boxes/${boxId}/${messageId}/audio.wav`;
                await this.storageRepo.uploadFromLocal(outAudioPath, destPath, 'audio/wav');
                storagePaths['audio'] = destPath;
            }
            // Cleanup temp files on storage (optional, but good practice)
            await this.storageRepo.deleteDirectory(`temp/${boxId}/${messageId}/`).catch(console.error);
            // Update message status
            await this.msgRepo.updateMessage(boxId, messageId, {
                status: 'ready',
                storagePaths
            });
            // Update Box Polling Cache
            await this.boxRepo.updatePollingCache(boxId, {
                hasNewMessage: true,
                latestMessageId: messageId
            });
        }
        catch (error) {
            console.error('Media processing failed:', error);
            await this.msgRepo.updateMessage(boxId, messageId, { status: 'failed' });
        }
        finally {
            // Cleanup local temp files
            if (fs.existsSync(tempDir)) {
                fs.rmSync(tempDir, { recursive: true, force: true });
            }
        }
    }
    async processImage(inputPath, outputPath) {
        const { width, height } = config_1.config.display;
        const buffer = await (0, sharp_1.default)(inputPath)
            .resize(width, height, { fit: 'cover' })
            .raw()
            .toBuffer();
        // Convert RGB888 to RGB565 (Little Endian for ESP32)
        const rgb565Buffer = Buffer.alloc(width * height * 2);
        for (let i = 0; i < width * height; i++) {
            const r = buffer[i * 3];
            const g = buffer[i * 3 + 1];
            const b = buffer[i * 3 + 2];
            const rgb565 = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
            // Little Endian
            rgb565Buffer.writeUInt16LE(rgb565, i * 2);
        }
        fs.writeFileSync(outputPath, rgb565Buffer);
    }
    processAudio(inputPath, outputPath) {
        return new Promise((resolve, reject) => {
            const { sampleRate, channels, format } = config_1.config.audio;
            (0, fluent_ffmpeg_1.default)(inputPath)
                .audioFrequency(sampleRate)
                .audioChannels(channels)
                .format(format)
                .on('end', () => resolve())
                .on('error', (err) => reject(err))
                .save(outputPath);
        });
    }
    extractVideoFrames(inputPath, outputDir) {
        return new Promise((resolve, reject) => {
            const { framesPerSecond, maxDurationSeconds } = config_1.config.video;
            (0, fluent_ffmpeg_1.default)(inputPath)
                .fps(framesPerSecond)
                .duration(maxDurationSeconds)
                .on('end', () => {
                // Read all extracted frames
                const files = fs.readdirSync(outputDir)
                    .filter(f => f.endsWith('.png'))
                    .sort() // Ensure sequential order
                    .map(f => path.join(outputDir, f));
                resolve(files);
            })
                .on('error', (err) => reject(err))
                .save(path.join(outputDir, 'frame_%04d.png'));
        });
    }
}
exports.MediaProcessingService = MediaProcessingService;
//# sourceMappingURL=media-processing.service.js.map