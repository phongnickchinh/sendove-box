import { NextFunction, Response } from 'express';
import { AuthenticatedRequest, ApiResponse } from '../types/api.types';
import { DeviceService } from '../services/device.service';

export class DeviceController {
  private deviceService: DeviceService;

  constructor() {
    this.deviceService = new DeviceService();
  }

  public register = async (req: AuthenticatedRequest, res: Response<ApiResponse>, next: NextFunction) => {
    try {
      const data = await this.deviceService.registerDevice(req.body);
      res.status(201).json({ success: true, data });
    } catch (error) {
      next(error);
    }
  };

  public poll = async (req: AuthenticatedRequest, res: Response<ApiResponse>, next: NextFunction) => {
    try {
      const boxId = `box_${req.deviceId}`;
      const data = await this.deviceService.poll(boxId);
      res.status(200).json({ success: true, data });
    } catch (error) {
      next(error);
    }
  };

  public heartbeat = async (req: AuthenticatedRequest, res: Response<ApiResponse>, next: NextFunction) => {
    try {
      const boxId = `box_${req.deviceId}`;
      await this.deviceService.heartbeat(boxId, req.body);
      res.status(200).json({ success: true, data: { serverTime: new Date().toISOString() } });
    } catch (error) {
      next(error);
    }
  };
}
