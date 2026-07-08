import { Router } from 'express';
import { MessageController } from '../controllers/message.controller';
import { requireAuth } from '../middleware/auth.middleware';
import { requireRole } from '../middleware/role-guard.middleware';
import { messageSendRateLimit } from '../middleware/rate-limiter.middleware';
import { validate, initiateMessageSchema, confirmMessageSchema } from '../middleware/validation.middleware';

export default function messageRoutes(controller: MessageController) {
  const router = Router({ mergeParams: true }); // /boxes/:boxId/messages

  router.use(requireAuth);

  // Sender: Bước 1 — Yêu cầu tạo message, nhận upload URLs (Rate Limited + Validated)
  router.post('/initiate', requireRole('sender'), messageSendRateLimit, validate(initiateMessageSchema), controller.initiateMessage);

  // Sender: Bước 2 — Upload xong, xác nhận ghi message vào RTDB (Validated)
  router.post('/confirm', requireRole('sender'), validate(confirmMessageSchema), controller.confirmMessage);

  // Receiver: Xem lịch sử tin nhắn
  router.get('/', requireRole('receiver'), controller.getMessages);
  
  // Receiver: Xem chi tiết 1 tin nhắn
  router.get('/:msgId', requireRole('receiver'), controller.getMessageDetails);

  return router;
}
