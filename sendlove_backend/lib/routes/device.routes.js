"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.default = deviceRoutes;
const express_1 = require("express");
const device_auth_middleware_1 = require("../middleware/device-auth.middleware");
const device_provisioning_middleware_1 = require("../middleware/device-provisioning.middleware");
const validation_middleware_1 = require("../middleware/validation.middleware");
function deviceRoutes(controller) {
    const router = (0, express_1.Router)();
    // Registration requires provisioning key (gắn trong firmware ESP32)
    router.post('/register', device_provisioning_middleware_1.requireProvisioningKey, (0, validation_middleware_1.validate)(validation_middleware_1.registerDeviceSchema), controller.register);
    // All subsequent ESP32 endpoints require the X-Device-Id and X-Device-Secret headers
    router.use(device_auth_middleware_1.requireDeviceAuth);
    router.get('/poll', controller.poll);
    router.post('/heartbeat', (0, validation_middleware_1.validate)(validation_middleware_1.heartbeatSchema), controller.heartbeat);
    // Note: /download is usually handled by returning a signed URL in /poll
    // but if you want to proxy it through functions:
    // router.get('/download/:mediaType', controller.downloadMedia);
    return router;
}
//# sourceMappingURL=device.routes.js.map