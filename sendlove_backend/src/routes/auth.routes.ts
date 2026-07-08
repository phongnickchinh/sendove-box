import { Router } from 'express';
import { AuthController } from '../controllers/auth.controller';
import { requireAuth } from '../middleware/auth.middleware';

export default function authRoutes(controller: AuthController) {
  const router = Router();

  // POST /auth/google is mostly handled by Frontend + Firebase SDK, but we can expose it if needed
  router.post('/google', requireAuth, controller.login);

  router.delete('/account', requireAuth, controller.deleteAccount);

  return router;
}
