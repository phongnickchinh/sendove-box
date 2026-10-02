import { Response, NextFunction } from 'express';
import { AuthenticatedRequest, ApiResponse } from '../types/api.types';
import { AppError } from './error-handler.middleware';
import { FirebaseRateLimitRepository } from '../repositories/firebase/firebase-rate-limit.repository';
import { config } from '../config';

const rateLimitRepo = new FirebaseRateLimitRepository();

/** Rate limiter for sending messages: N per 24-hour window per (sender, box) pair; 429 beyond. */
export const messageSendRateLimit = async (
  req: AuthenticatedRequest,
  res: Response<ApiResponse>,
  next: NextFunction
) => {
  try {
    const senderId = req.user?.uid;
    const boxId = req.params.boxId;

    if (!senderId) throw new AppError(401, 'unauthorized', 'User not authenticated');
    if (!boxId) throw new AppError(400, 'bad_request', 'Missing boxId');

    const { maxMessagesPerWindow, windowDurationMs } = config.rateLimit;

    // Atomic: check + increment in a single transaction → no race condition
    const result = await rateLimitRepo.checkAndIncrement(
      senderId, boxId, maxMessagesPerWindow, windowDurationMs
    );

    if (!result.allowed) {
      const remainingHours = Math.ceil((result.remainingMs || 0) / (60 * 60 * 1000));
      throw new AppError(
        429,
        'rate_limit_exceeded',
        `Bạn đã gửi tối đa ${maxMessagesPerWindow} tin nhắn. Vui lòng thử lại sau ${remainingHours} giờ.`
      );
    }

    next();
  } catch (error) {
    next(error);
  }
};
