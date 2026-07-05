"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.MusicService = void 0;
class MusicService {
    constructor() {
        // Mock music library. In reality, you'd store this in RTDB or Firestore
        this.mockLibrary = [
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
    }
    async getMusicLibrary() {
        return this.mockLibrary;
    }
    async getPreviewUrl(musicId) {
        const track = this.mockLibrary.find(m => m.musicId === musicId);
        return track ? track.previewURL : null;
    }
}
exports.MusicService = MusicService;
//# sourceMappingURL=music.service.js.map