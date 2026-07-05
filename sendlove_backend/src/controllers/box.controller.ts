import { NextFunction, Response } from 'express';
import { AuthenticatedRequest, ApiResponse } from '../types/api.types';
import { BoxService } from '../services/box.service';

export class BoxController {
  private boxService: BoxService;

  constructor() {
    this.boxService = new BoxService();
  }

  public pairBox = async (req: AuthenticatedRequest, res: Response<ApiResponse>, next: NextFunction) => {
    try {
      const uid = req.user!.uid;
      const { pairingCode } = req.body;
      const data = await this.boxService.pairBox(uid, pairingCode);
      res.status(200).json({ success: true, data });
    } catch (error) {
      next(error);
    }
  };

  public unpairBox = async (req: AuthenticatedRequest, res: Response<ApiResponse>, next: NextFunction) => {
    try {
      const uid = req.user!.uid;
      const { boxId } = req.params;
      const role = await this.boxService.unpairBox(uid, boxId);
      res.status(200).json({ success: true, data: { boxId, unpairedRole: role } });
    } catch (error) {
      next(error);
    }
  };

  public getBoxDetails = async (req: AuthenticatedRequest, res: Response<ApiResponse>, next: NextFunction) => {
    try {
      const uid = req.user!.uid;
      const { boxId } = req.params;
      const data = await this.boxService.getBoxDetails(uid, boxId);
      res.status(200).json({ success: true, data });
    } catch (error) {
      next(error);
    }
  };

  public updateWifi = async (req: AuthenticatedRequest, res: Response<ApiResponse>, next: NextFunction) => {
    try {
      const uid = req.user!.uid;
      const { boxId } = req.params;
      const { ssid, password } = req.body;
      
      await this.boxService.updateWifi(uid, boxId, ssid, password);
      
      res.status(200).json({ 
        success: true, 
        data: { status: 'pending', message: 'WiFi config queued. Box will apply on next poll.' } 
      });
    } catch (error) {
      next(error);
    }
  };
}
