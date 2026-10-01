import { Request, Response, NextFunction } from 'express';
import { AppError } from './error-handler.middleware';

/**
 * Authenticates a newly registering device with the provisioning key.
 * The ESP32 firmware must send it in the 'X-Provisioning-Key' header when
 * calling /device/register.
 *
 * The key is configured through the DEVICE_PROVISIONING_KEY env variable.
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
