export interface BackgroundMusic {
  musicId: string;
  name: string;
  category: string;
  durationSeconds: number;
  previewURL: string;
}

export class MusicService {
  // Mock music library. In reality, you'd store this in RTDB or Firestore
  private mockLibrary: BackgroundMusic[] = [
    {
      musicId: 'bgm_001',
      name: 'Morning Sunrise',
      category: 'calm',
      durationSeconds: 30,
      previewURL: 'https://storage.googleapis.com/.../bgm_001.mp3'
    },
    {
      musicId: 'bgm_002',
      name: 'Happy Birthday',
      category: 'celebration',
      durationSeconds: 15,
      previewURL: 'https://storage.googleapis.com/.../bgm_002.mp3'
    }
  ];

  async getMusicLibrary(): Promise<BackgroundMusic[]> {
    return this.mockLibrary;
  }

  async getPreviewUrl(musicId: string): Promise<string | null> {
    const track = this.mockLibrary.find(m => m.musicId === musicId);
    return track ? track.previewURL : null;
  }
}
