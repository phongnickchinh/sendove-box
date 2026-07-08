import { Response, NextFunction } from 'express';
import { AuthenticatedRequest, ApiResponse } from '../types/api.types';
import { AppError } from './error-handler.middleware';
import { FirebaseRateLimitRepository } from '../repositories/firebase/firebase-rate-limit.repository';
import { config } from '../config';

const rateLimitRepo = new FirebaseRateLimitRepository();

/**
 * Rate Limiter Middleware cho việc gửi tin nhắn.
 * Giới hạn: N tin / 24 giờ cho mỗi cặp (sender_id + box_id).
 *
 * Logic:
 * 1. Đọc record rate_limits/{senderId}_{boxId}
 * 2. Nếu chưa có → tạo mới (count=1, window_start=now) → cho qua
 * 3. Nếu window hết hạn (now - window_start > 24h) → reset → cho qua
 * 4. Nếu count >= LIMIT → trả 429
 * 5. Nếu dưới limit → tăng count → cho qua
 */
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

    // Atomic: check + increment trong 1 transaction duy nhất → tránh race condition
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
