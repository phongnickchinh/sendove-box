import { NextFunction, Response } from 'express';
import { AuthenticatedRequest, ApiResponse } from '../types/api.types';
import { MusicService } from '../services/music.service';

export class MusicController {
  private musicService: MusicService;

  constructor() {
    this.musicService = new MusicService();
  }

  public getMusicLibrary = async (req: AuthenticatedRequest, res: Response<ApiResponse>, next: NextFunction) => {
    try {
      const data = await this.musicService.getMusicLibrary();
      res.status(200).json({ success: true, data });
    } catch (error) {
      next(error);
    }
  };

  public getPreviewUrl = async (req: AuthenticatedRequest, res: Response<ApiResponse>, next: NextFunction) => {
    try {
      const { musicId } = req.params;
      const url = await this.musicService.getPreviewUrl(musicId);
      if (!url) {
        res.status(404).json({ success: false, error: { code: 'not_found', message: 'Music track not found' } });
        return;
      }
      res.status(200).json({ success: true, data: { previewURL: url } });
    } catch (error) {
      next(error);
    }
  };
}
