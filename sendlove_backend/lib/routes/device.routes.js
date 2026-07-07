"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
const express_1 = require("express");
const device_controller_1 = require("../controllers/device.controller");
const device_auth_middleware_1 = require("../middleware/device-auth.middleware");
const router = (0, express_1.Router)();
const controller = new device_controller_1.DeviceController();
// Registration doesn't require device auth (because the device hasn't got the secret yet)
// But in a real scenario, you'd want some initial handshake security
router.post('/register', controller.register);
// All subsequent ESP32 endpoints require the X-Device-Id and X-Device-Secret headers
router.use(device_auth_middleware_1.requireDeviceAuth);
router.get('/poll', controller.poll);
router.post('/heartbeat', controller.heartbeat);
// Note: /download is usually handled by returning a signed URL in /poll
// but if you want to proxy it through functions:
// router.get('/download/:msgId/:fileType', controller.downloadFile);
exports.default = router;
//# sourceMappingURL=device.routes.js.map