export type MessageType = 
  | 'text' 
  | 'text_music' 
  | 'photo' 
  | 'photo_music' 
  | 'photo_voice' 
  | 'voice' 
  | 'video' 
  | 'video_music' 
  | 'video_voice';

export type MessageStatus = 'awaiting_upload' | 'processing' | 'ready' | 'delivered' | 'failed';

export interface Message {
  messageId: string;
  type: MessageType;
  status: MessageStatus;
  createdAt: number;
  text?: string;
  metadata?: {
    photoCount?: number;
    hasAudio?: boolean;
    slideshowInterval?: number; // seconds
  };
  storagePaths?: {
    [key: string]: string; // e.g. "frame_0": "path/to/frame_0.bin", "audio": "path/to/audio.wav"
  };
  thumbnailURL?: string; // For Web App history
}
