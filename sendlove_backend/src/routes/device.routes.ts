import { Router } from 'express';
import { DeviceController } from '../controllers/device.controller';
import { requireDeviceAuth } from '../middleware/device-auth.middleware';
import { requireProvisioningKey } from '../middleware/device-provisioning.middleware';
import { validate, registerDeviceSchema, heartbeatSchema } from '../middleware/validation.middleware';

export default function deviceRoutes(controller: DeviceController) {
  const router = Router();

  // Registration requires the provisioning key (built into the ESP32 firmware)
  router.post('/register', requireProvisioningKey, validate(registerDeviceSchema), controller.register);

  // All subsequent ESP32 endpoints require the X-Device-Id and X-Device-Secret headers
  router.use(requireDeviceAuth);

  router.get('/poll', controller.poll);
  router.post('/heartbeat', validate(heartbeatSchema), controller.heartbeat);


  return router;
}
