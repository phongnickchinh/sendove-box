import { NextFunction, Response } from 'express';
import { AuthenticatedRequest, ApiResponse } from '../types/api.types';
import { AlarmService } from '../services/alarm.service';

export class AlarmController {
  constructor(
    private alarmService: AlarmService = new AlarmService()
  ) {}

  public createAlarm = async (req: AuthenticatedRequest, res: Response<ApiResponse>, next: NextFunction) => {
    try {
      const { boxId } = req.params;
      const data = await this.alarmService.createAlarm(boxId, req.body);
      res.status(201).json({ success: true, data });
    } catch (error) {
      next(error);
    }
  };

  public getAlarms = async (req: AuthenticatedRequest, res: Response<ApiResponse>, next: NextFunction) => {
    try {
      const { boxId } = req.params;
      const data = await this.alarmService.getAlarms(boxId);
      res.status(200).json({ success: true, data });
    } catch (error) {
      next(error);
    }
  };

  public updateAlarm = async (req: AuthenticatedRequest, res: Response<ApiResponse>, next: NextFunction) => {
    try {
      const { boxId, alarmId } = req.params;
      const data = await this.alarmService.updateAlarm(boxId, alarmId, req.body);
      res.status(200).json({ success: true, data });
    } catch (error) {
      next(error);
    }
  };

  public deleteAlarm = async (req: AuthenticatedRequest, res: Response<ApiResponse>, next: NextFunction) => {
    try {
      const { boxId, alarmId } = req.params;
      await this.alarmService.deleteAlarm(boxId, alarmId);
      res.status(200).json({ success: true, data: null });
    } catch (error) {
      next(error);
    }
  };
}
