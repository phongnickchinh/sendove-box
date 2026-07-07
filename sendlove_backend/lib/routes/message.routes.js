"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
const express_1 = require("express");
const message_controller_1 = require("../controllers/message.controller");
const auth_middleware_1 = require("../middleware/auth.middleware");
const role_guard_middleware_1 = require("../middleware/role-guard.middleware");
const rate_limiter_middleware_1 = require("../middleware/rate-limiter.middleware");
const router = (0, express_1.Router)({ mergeParams: true }); // /boxes/:boxId/messages
const controller = new message_controller_1.MessageController();
router.use(auth_middleware_1.requireAuth);
// Sender: Bước 1 — Yêu cầu tạo message, nhận upload URLs (Rate Limited)
router.post('/initiate', (0, role_guard_middleware_1.requireRole)('sender'), rate_limiter_middleware_1.messageSendRateLimit, controller.initiateMessage);
// Sender: Bước 2 — Upload xong, xác nhận ghi message vào RTDB
router.post('/confirm', (0, role_guard_middleware_1.requireRole)('sender'), controller.confirmMessage);
// Receiver: Xem lịch sử tin nhắn
router.get('/', (0, role_guard_middleware_1.requireRole)('receiver'), controller.getMessages);
// Receiver: Xem chi tiết 1 tin nhắn
router.get('/:msgId', (0, role_guard_middleware_1.requireRole)('receiver'), controller.getMessageDetails);
exports.default = router;
//# sourceMappingURL=message.routes.js.map