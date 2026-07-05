import { Router } from 'express';
import { DeviceController } from '../controllers/device.controller';
import { requireDeviceAuth } from '../middleware/device-auth.middleware';

const router = Router();
const controller = new DeviceController();

// Registration doesn't require device auth (because the device hasn't got the secret yet)
// But in a real scenario, you'd want some initial handshake security
router.post('/register', controller.register);

// All subsequent ESP32 endpoints require the X-Device-Id and X-Device-Secret headers
router.use(requireDeviceAuth);

router.get('/poll', controller.poll);
router.post('/heartbeat', controller.heartbeat);
router.post('/ack/:msgId', controller.ackMessage);

// Note: /download is usually handled by returning a signed URL in /poll
// but if you want to proxy it through functions:
// router.get('/download/:msgId/:fileType', controller.downloadFile);

export default router;
