import { Request, Response, NextFunction } from 'express';
import { AppError } from './error-handler.middleware';

/**
 * Middleware xác thực thiết bị mới đăng ký bằng provisioning key.
 * ESP32 firmware phải gắn key này vào header 'X-Provisioning-Key' khi gọi /device/register.
 *
 * Key được cấu hình qua biến môi trường DEVICE_PROVISIONING_KEY.
 */
export const requireProvisioningKey = (req: Request, _res: Response, next: NextFunction) => {
  const key = req.headers['x-provisioning-key'] as string;
  const expectedKey = process.env.DEVICE_PROVISIONING_KEY;

  if (!expectedKey) {
    console.warn('[Provisioning] DEVICE_PROVISIONING_KEY is not set. Rejecting all device registrations.');
    return next(new AppError(500, 'server_config_error', 'Device provisioning is not configured'));
  }

  if (!key || key !== expectedKey) {
    return next(new AppError(401, 'unauthorized', 'Invalid or missing provisioning key'));
  }

  next();
};
