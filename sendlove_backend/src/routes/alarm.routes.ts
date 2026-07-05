import { Router } from 'express';
import { AlarmController } from '../controllers/alarm.controller';
import { requireAuth } from '../middleware/auth.middleware';
import { requireRole } from '../middleware/role-guard.middleware';

const router = Router({ mergeParams: true });
const controller = new AlarmController();

// Note: role guard should allow both sender and receiver to list alarms, but maybe only receiver creates
// Or based on requirements, "Receiver tự cài" so Receiver role for all
router.use(requireAuth);
router.use(requireRole('receiver'));

router.post('/', controller.createAlarm);
router.get('/', controller.getAlarms);
router.patch('/:alarmId', controller.updateAlarm);
router.delete('/:alarmId', controller.deleteAlarm);

export default router;
