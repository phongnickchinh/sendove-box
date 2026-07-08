"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.default = boxRoutes;
const express_1 = require("express");
const auth_middleware_1 = require("../middleware/auth.middleware");
const validation_middleware_1 = require("../middleware/validation.middleware");
function boxRoutes(controller, messageRouter, alarmRouter) {
    const router = (0, express_1.Router)();
    router.use(auth_middleware_1.requireAuth);
    router.post('/pair', (0, validation_middleware_1.validate)(validation_middleware_1.pairBoxSchema), controller.pairBox);
    router.delete('/:boxId/unpair', controller.unpairBox);
    router.get('/:boxId', controller.getBoxDetails);
    router.put('/:boxId/wifi', (0, validation_middleware_1.validate)(validation_middleware_1.updateWifiSchema), controller.updateWifi);
    // Mount nested routes
    router.use('/:boxId/messages', messageRouter);
    router.use('/:boxId/alarms', alarmRouter);
    return router;
}
//# sourceMappingURL=box.routes.js.map