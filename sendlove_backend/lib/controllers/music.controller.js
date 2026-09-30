"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.MusicController = void 0;
const music_service_1 = require("../services/music.service");
class MusicController {
    constructor(musicService = new music_service_1.MusicService()) {
        this.musicService = musicService;
        this.list = async (req, res, next) => {
            try {
                const data = await this.musicService.list(req.params.boxId);
                res.status(200).json({ success: true, data });
            }
            catch (error) {
                next(error);
            }
        };
        this.initiateUpload = async (req, res, next) => {
            try {
                const data = await this.musicService.initiateUpload(req.params.boxId, req.body?.music_id);
                res.status(200).json({ success: true, data });
            }
            catch (error) {
                next(error);
            }
        };
        this.commit = async (req, res, next) => {
            try {
                const data = await this.musicService.commit(req.params.boxId, req.user.uid, req.body);
                res.status(200).json({ success: true, data });
            }
            catch (error) {
                next(error);
            }
        };
        this.rename = async (req, res, next) => {
            try {
                await this.musicService.rename(req.params.boxId, req.params.musicId, req.body?.name);
                res.status(200).json({ success: true, data: null });
            }
            catch (error) {
                next(error);
            }
        };
        this.remove = async (req, res, next) => {
            try {
                const data = await this.musicService.remove(req.params.boxId, req.params.musicId);
                res.status(200).json({ success: true, data });
            }
            catch (error) {
                next(error);
            }
        };
        this.previewUrl = async (req, res, next) => {
            try {
                const url = await this.musicService.previewUrl(req.params.boxId, req.params.musicId);
                res.status(200).json({ success: true, data: { url } });
            }
            catch (error) {
                next(error);
            }
        };
    }
}
exports.MusicController = MusicController;
//# sourceMappingURL=music.controller.js.map