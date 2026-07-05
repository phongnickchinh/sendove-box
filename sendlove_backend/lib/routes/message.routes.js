"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
const express_1 = require("express");
const message_controller_1 = require("../controllers/message.controller");
const auth_middleware_1 = require("../middleware/auth.middleware");
const role_guard_middleware_1 = require("../middleware/role-guard.middleware");
const router = (0, express_1.Router)({ mergeParams: true }); // Important for nested routes like /boxes/:boxId/messages
const controller = new message_controller_1.MessageController();
router.use(auth_middleware_1.requireAuth);
// Only sender can create messages
router.post('/', (0, role_guard_middleware_1.requireRole)('sender'), controller.createMessage);
router.post('/:msgId/complete', (0, role_guard_middleware_1.requireRole)('sender'), controller.completeUpload);
// Sender or receiver can view history? Requirement says "Lịch sử tin nhắn chỉ lưu trên Web App" (usually Sender views history)
router.get('/', (0, role_guard_middleware_1.requireRole)('sender'), controller.getMessages);
router.get('/:msgId', (0, role_guard_middleware_1.requireRole)('sender'), controller.getMessageDetails);
exports.default = router;
//# sourceMappingURL=message.routes.js.map