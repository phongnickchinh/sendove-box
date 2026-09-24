import { NextFunction, Response } from 'express';
import { AuthenticatedRequest, ApiResponse } from '../types/api.types';
import { MusicService } from '../services/music.service';

export class MusicController {
  constructor(
    private musicService: MusicService = new MusicService()
  ) {}

  public list = async (req: AuthenticatedRequest, res: Response<ApiResponse>, next: NextFunction) => {
    try {
      const data = await this.musicService.list(req.params.boxId);
      res.status(200).json({ success: true, data });
    } catch (error) {
      next(error);
    }
  };

  public initiateUpload = async (req: AuthenticatedRequest, res: Response<ApiResponse>, next: NextFunction) => {
    try {
      const data = await this.musicService.initiateUpload(req.params.boxId, req.body?.music_id);
      res.status(200).json({ success: true, data });
    } catch (error) {
      next(error);
    }
  };

  public commit = async (req: AuthenticatedRequest, res: Response<ApiResponse>, next: NextFunction) => {
    try {
      const data = await this.musicService.commit(req.params.boxId, req.user!.uid, req.body);
      res.status(200).json({ success: true, data });
    } catch (error) {
      next(error);
    }
  };

  public rename = async (req: AuthenticatedRequest, res: Response<ApiResponse>, next: NextFunction) => {
    try {
      await this.musicService.rename(req.params.boxId, req.params.musicId, req.body?.name);
      res.status(200).json({ success: true, data: null });
    } catch (error) {
      next(error);
    }
  };

  public remove = async (req: AuthenticatedRequest, res: Response<ApiResponse>, next: NextFunction) => {
    try {
      const data = await this.musicService.remove(req.params.boxId, req.params.musicId);
      res.status(200).json({ success: true, data });
    } catch (error) {
      next(error);
    }
  };

  public previewUrl = async (req: AuthenticatedRequest, res: Response<ApiResponse>, next: NextFunction) => {
    try {
      const url = await this.musicService.previewUrl(req.params.boxId, req.params.musicId);
      res.status(200).json({ success: true, data: { url } });
    } catch (error) {
      next(error);
    }
  };
}
