import { Response, NextFunction } from 'express';
import { AuthenticatedRequest, ApiResponse } from '../types/api.types';
import { AppError } from './error-handler.middleware';
import { FirebaseBoxRepository } from '../repositories/firebase/firebase-box.repository';

const boxRepo = new FirebaseBoxRepository();

export const requireRole = (requiredRole: 'sender' | 'receiver') => {
  return async (req: AuthenticatedRequest, res: Response<ApiResponse>, next: NextFunction) => {
    try {
      const uid = req.user?.uid;
      const boxId = req.params.boxId;

      if (!uid) throw new AppError(401, 'unauthorized', 'User not authenticated');
      if (!boxId) throw new AppError(400, 'bad_request', 'Missing boxId');

      const box = await boxRepo.getById(boxId);
      if (!box) throw new AppError(404, 'box_not_found', 'Box not found');

      if (requiredRole === 'sender' && box.pairing.sender_id !== uid) {
        throw new AppError(403, 'forbidden', 'Only sender can perform this action');
      }

      if (requiredRole === 'receiver' && box.pairing.receiver_id !== uid) {
        throw new AppError(403, 'forbidden', 'Only receiver can perform this action');
      }

      next();
    } catch (error) {
      next(error);
    }
  };
};
