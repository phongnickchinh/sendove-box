import { Router } from 'express';
import { BoxController } from '../controllers/box.controller';
import { requireAuth } from '../middleware/auth.middleware';
import { validate, pairBoxSchema, updateWifiSchema } from '../middleware/validation.middleware';

export default function boxRoutes(
  controller: BoxController,
  messageRouter: Router,
  alarmRouter: Router
) {
  const router = Router();

  router.use(requireAuth);

  router.post('/pair', validate(pairBoxSchema), controller.pairBox);
  router.delete('/:boxId/unpair', controller.unpairBox);
  router.get('/:boxId', controller.getBoxDetails);
  router.put('/:boxId/wifi', validate(updateWifiSchema), controller.updateWifi);

  // Mount nested routes
  router.use('/:boxId/messages', messageRouter);
  router.use('/:boxId/alarms', alarmRouter);

  return router;
}
