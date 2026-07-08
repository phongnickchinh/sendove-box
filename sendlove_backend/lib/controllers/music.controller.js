"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.MusicController = void 0;
const music_service_1 = require("../services/music.service");
class MusicController {
    constructor(musicService = new music_service_1.MusicService()) {
        this.musicService = musicService;
        this.getMusicLibrary = async (req, res, next) => {
            try {
                const data = await this.musicService.getMusicLibrary();
                res.status(200).json({ success: true, data });
            }
            catch (error) {
                next(error);
            }
        };
        this.getPreviewUrl = async (req, res, next) => {
            try {
                const { musicId } = req.params;
                const url = await this.musicService.getPreviewUrl(musicId);
                if (!url) {
                    res.status(404).json({ success: false, error: { code: 'not_found', message: 'Music track not found' } });
                    return;
                }
                res.status(200).json({ success: true, data: { previewURL: url } });
            }
            catch (error) {
                next(error);
            }
        };
    }
}
exports.MusicController = MusicController;
//# sourceMappingURL=music.controller.js.map