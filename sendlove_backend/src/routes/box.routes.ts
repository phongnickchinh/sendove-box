import { Router } from 'express';
import { BoxController } from '../controllers/box.controller';
import { requireAuth } from '../middleware/auth.middleware';
import { validate, pairBoxSchema, updateWifiSchema, updateBoxConfigSchema } from '../middleware/validation.middleware';

export default function boxRoutes(
  controller: BoxController,
  messageRouter: Router,
  alarmRouter: Router,
  themeRouter: Router,
  musicRouter: Router
) {
  const router = Router();

  router.use(requireAuth);

  router.post('/pair', validate(pairBoxSchema), controller.pairBox);
  router.delete('/:boxId/unpair', controller.unpairBox);
  router.get('/:boxId', controller.getBoxDetails);
  router.put('/:boxId/wifi', validate(updateWifiSchema), controller.updateWifi);
  router.put('/:boxId/config', validate(updateBoxConfigSchema), controller.updateBoxConfig);

  // Mount nested routes
  router.use('/:boxId/messages', messageRouter);
  router.use('/:boxId/alarms', alarmRouter);
  router.use('/:boxId/theme', themeRouter);
  router.use('/:boxId/music', musicRouter);

  return router;
}
