import { Router } from 'express';
import { MessageController } from '../controllers/message.controller';
import { requireAuth } from '../middleware/auth.middleware';
import { requireRole } from '../middleware/role-guard.middleware';

const router = Router({ mergeParams: true }); // Important for nested routes like /boxes/:boxId/messages
const controller = new MessageController();

router.use(requireAuth);

// Only sender can create messages
router.post('/', requireRole('sender'), controller.createMessage);
router.post('/:msgId/complete', requireRole('sender'), controller.completeUpload);

// Sender or receiver can view history? Requirement says "Lịch sử tin nhắn chỉ lưu trên Web App" (usually Sender views history)
router.get('/', requireRole('sender'), controller.getMessages);
router.get('/:msgId', requireRole('sender'), controller.getMessageDetails);

export default router;
