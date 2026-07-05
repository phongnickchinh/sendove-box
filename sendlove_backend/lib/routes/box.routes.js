"use strict";
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
const express_1 = require("express");
const box_controller_1 = require("../controllers/box.controller");
const auth_middleware_1 = require("../middleware/auth.middleware");
const message_routes_1 = __importDefault(require("./message.routes"));
const alarm_routes_1 = __importDefault(require("./alarm.routes"));
const router = (0, express_1.Router)();
const controller = new box_controller_1.BoxController();
router.use(auth_middleware_1.requireAuth);
router.post('/pair', controller.pairBox);
router.delete('/:boxId/unpair', controller.unpairBox);
router.get('/:boxId', controller.getBoxDetails);
router.put('/:boxId/wifi', controller.updateWifi);
// Mount nested routes
router.use('/:boxId/messages', message_routes_1.default);
router.use('/:boxId/alarms', alarm_routes_1.default);
exports.default = router;
//# sourceMappingURL=box.routes.js.map