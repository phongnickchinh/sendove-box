import { Router } from 'express';
import { MessageController } from '../controllers/message.controller';
import { requireAuth } from '../middleware/auth.middleware';
import { requireRole } from '../middleware/role-guard.middleware';
import { messageSendRateLimit } from '../middleware/rate-limiter.middleware';

const router = Router({ mergeParams: true }); // /boxes/:boxId/messages
const controller = new MessageController();

router.use(requireAuth);

// Sender: Bước 1 — Yêu cầu tạo message, nhận upload URLs (Rate Limited)
router.post('/initiate', requireRole('sender'), messageSendRateLimit, controller.initiateMessage);

// Sender: Bước 2 — Upload xong, xác nhận ghi message vào RTDB
router.post('/confirm', requireRole('sender'), controller.confirmMessage);

// Receiver: Xem lịch sử tin nhắn
router.get('/', requireRole('receiver'), controller.getMessages);

// Receiver: Xem chi tiết 1 tin nhắn
router.get('/:msgId', requireRole('receiver'), controller.getMessageDetails);

export default router;
