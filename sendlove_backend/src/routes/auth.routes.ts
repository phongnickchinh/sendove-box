import { Router } from 'express';
import { AuthController } from '../controllers/auth.controller';
import { requireAuth } from '../middleware/auth.middleware';

const router = Router();
const controller = new AuthController();

// POST /auth/google is mostly handled by Frontend + Firebase SDK, but we can expose it if needed
router.post('/google', requireAuth, controller.login);

router.delete('/account', requireAuth, controller.deleteAccount);

export default router;
