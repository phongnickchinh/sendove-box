import { FirebaseStorageRepository } from '../repositories/firebase/firebase-storage.repository';
import { FirebaseMessageRepository } from '../repositories/firebase/firebase-message.repository';
import { FirebaseBoxRepository } from '../repositories/firebase/firebase-box.repository';
import { MessageType } from '../types/message.types';
import { config } from '../config';
import * as os from 'os';
import * as path from 'path';
import * as fs from 'fs';
import ffmpeg from 'fluent-ffmpeg';
import ffmpegInstaller from '@ffmpeg-installer/ffmpeg';
import sharp from 'sharp';

// Set ffmpeg path
ffmpeg.setFfmpegPath(ffmpegInstaller.path);

export class MediaProcessingService {
  private storageRepo: FirebaseStorageRepository;
  private msgRepo: FirebaseMessageRepository;
  private boxRepo: FirebaseBoxRepository;

  constructor() {
    this.storageRepo = new FirebaseStorageRepository();
    this.msgRepo = new FirebaseMessageRepository();
    this.boxRepo = new FirebaseBoxRepository();
  }

  async processMessageMedia(boxId: string, messageId: string, type: MessageType, uploadedFields: string[]): Promise<void> {
    const tempDir = path.join(os.tmpdir(), messageId);
    
    try {
      // Create temp dir
      if (!fs.existsSync(tempDir)) {
        fs.mkdirSync(tempDir, { recursive: true });
      }

      const storagePaths: Record<string, string> = {};

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
      let audioSourcePath: string | null = null;
      if (type.includes('voice') && uploadedFields.includes('voice')) {
        audioSourcePath = path.join(tempDir, 'voice_original');
        await this.storageRepo.downloadToLocal(`temp/${boxId}/${messageId}/voice`, audioSourcePath);
      } else if (type.includes('music')) {
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

    } catch (error) {
      console.error('Media processing failed:', error);
      await this.msgRepo.updateMessage(boxId, messageId, { status: 'failed' });
    } finally {
      // Cleanup local temp files
      if (fs.existsSync(tempDir)) {
        fs.rmSync(tempDir, { recursive: true, force: true });
      }
    }
  }

  private async processImage(inputPath: string, outputPath: string): Promise<void> {
    const { width, height } = config.display;
    
    const buffer = await sharp(inputPath)
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

  private processAudio(inputPath: string, outputPath: string): Promise<void> {
    return new Promise((resolve, reject) => {
      const { sampleRate, channels, format } = config.audio;
      
      ffmpeg(inputPath)
        .audioFrequency(sampleRate)
        .audioChannels(channels)
        .format(format)
        .on('end', () => resolve())
        .on('error', (err) => reject(err))
        .save(outputPath);
    });
  }

  private extractVideoFrames(inputPath: string, outputDir: string): Promise<string[]> {
    return new Promise((resolve, reject) => {
      const { framesPerSecond, maxDurationSeconds } = config.video;
      
      ffmpeg(inputPath)
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
