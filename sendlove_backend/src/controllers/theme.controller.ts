import { NextFunction, Response } from 'express';
import { AuthenticatedRequest, ApiResponse } from '../types/api.types';
import { ThemeService } from '../services/theme.service';

export class ThemeController {
  constructor(
    private themeService: ThemeService = new ThemeService()
  ) {}

  public getTheme = async (req: AuthenticatedRequest, res: Response<ApiResponse>, next: NextFunction) => {
    try {
      const data = await this.themeService.getTheme(req.params.boxId);
      res.status(200).json({ success: true, data });
    } catch (error) {
      next(error);
    }
  };

  public saveTheme = async (req: AuthenticatedRequest, res: Response<ApiResponse>, next: NextFunction) => {
    try {
      const data = await this.themeService.saveTheme(req.params.boxId, req.user!.uid, req.body);
      res.status(200).json({ success: true, data });
    } catch (error) {
      next(error);
    }
  };

  public initiateBackground = async (req: AuthenticatedRequest, res: Response<ApiResponse>, next: NextFunction) => {
    try {
      const data = await this.themeService.initiateBackgroundUpload(req.params.boxId);
      res.status(200).json({ success: true, data });
    } catch (error) {
      next(error);
    }
  };

  public initiateFont = async (req: AuthenticatedRequest, res: Response<ApiResponse>, next: NextFunction) => {
    try {
      const data = await this.themeService.initiateFontUpload(req.params.boxId);
      res.status(200).json({ success: true, data });
    } catch (error) {
      next(error);
    }
  };
}
