import { Router } from 'express';
import { BoxController } from '../controllers/box.controller';
import { requireAuth } from '../middleware/auth.middleware';
import messageRoutes from './message.routes';
import alarmRoutes from './alarm.routes';

const router = Router();
const controller = new BoxController();

router.use(requireAuth);

router.post('/pair', controller.pairBox);
router.delete('/:boxId/unpair', controller.unpairBox);
router.get('/:boxId', controller.getBoxDetails);
router.put('/:boxId/wifi', controller.updateWifi);

// Mount nested routes
router.use('/:boxId/messages', messageRoutes);
router.use('/:boxId/alarms', alarmRoutes);

export default router;
