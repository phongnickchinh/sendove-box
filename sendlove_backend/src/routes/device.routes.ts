import { Router } from 'express';
import { DeviceController } from '../controllers/device.controller';
import { requireDeviceAuth } from '../middleware/device-auth.middleware';
import { requireProvisioningKey } from '../middleware/device-provisioning.middleware';
import { validate, registerDeviceSchema, heartbeatSchema } from '../middleware/validation.middleware';

export default function deviceRoutes(controller: DeviceController) {
  const router = Router();

  // Registration requires provisioning key (gắn trong firmware ESP32)
  router.post('/register', requireProvisioningKey, validate(registerDeviceSchema), controller.register);

  // All subsequent ESP32 endpoints require the X-Device-Id and X-Device-Secret headers
  router.use(requireDeviceAuth);

  router.get('/poll', controller.poll);
  router.post('/heartbeat', validate(heartbeatSchema), controller.heartbeat);

  // Note: /download is usually handled by returning a signed URL in /poll
  // but if you want to proxy it through functions:
  // router.get('/download/:mediaType', controller.downloadMedia);

  return router;
}
