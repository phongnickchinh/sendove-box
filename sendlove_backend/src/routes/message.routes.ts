import { Router } from 'express';
import { MessageController } from '../controllers/message.controller';
import { requireAuth } from '../middleware/auth.middleware';
import { requireRole } from '../middleware/role-guard.middleware';
import { messageSendRateLimit } from '../middleware/rate-limiter.middleware';
import { validate, initiateMessageSchema, confirmMessageSchema } from '../middleware/validation.middleware';

export default function messageRoutes(controller: MessageController) {
  const router = Router({ mergeParams: true }); // /boxes/:boxId/messages

  router.use(requireAuth);

  // Sender, step 1 — request a message and get upload URLs (rate limited + validated)
  router.post('/initiate', requireRole('sender'), messageSendRateLimit, validate(initiateMessageSchema), controller.initiateMessage);

  // Sender, step 2 — uploads done; confirm and write the message to RTDB (validated)
  router.post('/confirm', requireRole('sender'), validate(confirmMessageSchema), controller.confirmMessage);

  // Sender & receiver: message history
  router.get('/', requireRole(['sender', 'receiver']), controller.getMessages);

  // Sender & receiver: one message's details
  router.get('/:msgId', requireRole(['sender', 'receiver']), controller.getMessageDetails);

  return router;
}
