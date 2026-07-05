import { Router } from 'express';
import { UserController } from '../controllers/user.controller';
import { requireAuth } from '../middleware/auth.middleware';

const router = Router();
const controller = new UserController();

router.use(requireAuth);

router.get('/me', controller.getProfile);
router.patch('/me', controller.updateProfile);

export default router;
