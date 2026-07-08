"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.default = alarmRoutes;
const express_1 = require("express");
const auth_middleware_1 = require("../middleware/auth.middleware");
const role_guard_middleware_1 = require("../middleware/role-guard.middleware");
const validation_middleware_1 = require("../middleware/validation.middleware");
function alarmRoutes(controller) {
    const router = (0, express_1.Router)({ mergeParams: true });
    // Note: role guard should allow both sender and receiver to list alarms, but maybe only receiver creates
    // Or based on requirements, "Receiver tự cài" so Receiver role for all
    router.use(auth_middleware_1.requireAuth);
    router.use((0, role_guard_middleware_1.requireRole)('receiver'));
    router.post('/', (0, validation_middleware_1.validate)(validation_middleware_1.createAlarmSchema), controller.createAlarm);
    router.get('/', controller.getAlarms);
    router.patch('/:alarmId', (0, validation_middleware_1.validate)(validation_middleware_1.updateAlarmSchema), controller.updateAlarm);
    router.delete('/:alarmId', controller.deleteAlarm);
    return router;
}
//# sourceMappingURL=alarm.routes.js.map