import { Response, NextFunction } from 'express';
import { AuthenticatedRequest, ApiResponse } from '../types/api.types';
import { AppError } from './error-handler.middleware';
import { FirebaseBoxRepository } from '../repositories/firebase/firebase-box.repository';

const boxRepo = new FirebaseBoxRepository();

export const requireDeviceAuth = async (req: AuthenticatedRequest, res: Response<ApiResponse>, next: NextFunction) => {
  try {
    const deviceId = req.headers['x-device-id'] as string;
    const deviceSecret = req.headers['x-device-secret'] as string;

    if (!deviceId || !deviceSecret) {
      throw new AppError(401, 'unauthorized', 'Missing device credentials');
    }

    const boxId = `box_${deviceId}`;
    const box = await boxRepo.getById(boxId);

    if (!box || box.device_secret !== deviceSecret) {
      throw new AppError(401, 'unauthorized', 'Invalid device credentials');
    }

    req.deviceId = deviceId;
    next();
  } catch (error) {
    next(error);
  }
};
