"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
const express_1 = require("express");
const alarm_controller_1 = require("../controllers/alarm.controller");
const auth_middleware_1 = require("../middleware/auth.middleware");
const role_guard_middleware_1 = require("../middleware/role-guard.middleware");
const router = (0, express_1.Router)({ mergeParams: true });
const controller = new alarm_controller_1.AlarmController();
// Note: role guard should allow both sender and receiver to list alarms, but maybe only receiver creates
// Or based on requirements, "Receiver tự cài" so Receiver role for all
router.use(auth_middleware_1.requireAuth);
router.use((0, role_guard_middleware_1.requireRole)('receiver'));
router.post('/', controller.createAlarm);
router.get('/', controller.getAlarms);
router.patch('/:alarmId', controller.updateAlarm);
router.delete('/:alarmId', controller.deleteAlarm);
exports.default = router;
//# sourceMappingURL=alarm.routes.js.map