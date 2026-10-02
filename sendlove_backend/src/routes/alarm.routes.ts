import { Router } from 'express';
import { AlarmController } from '../controllers/alarm.controller';
import { requireAuth } from '../middleware/auth.middleware';
import { requireRole } from '../middleware/role-guard.middleware';
import { validate, createAlarmSchema, updateAlarmSchema } from '../middleware/validation.middleware';

export default function alarmRoutes(controller: AlarmController) {
  const router = Router({ mergeParams: true });

  // Alarms are set by the receiver only, so every route requires the receiver role.
  router.use(requireAuth);
  router.use(requireRole('receiver'));

  router.post('/', validate(createAlarmSchema), controller.createAlarm);
  router.get('/', controller.getAlarms);
  router.patch('/:alarmId', validate(updateAlarmSchema), controller.updateAlarm);
  router.delete('/:alarmId', controller.deleteAlarm);

  return router;
}
