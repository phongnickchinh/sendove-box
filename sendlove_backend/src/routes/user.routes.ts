import { Router } from 'express';
import { UserController } from '../controllers/user.controller';
import { requireAuth } from '../middleware/auth.middleware';
import { validate, updateProfileSchema } from '../middleware/validation.middleware';

export default function userRoutes(controller: UserController) {
  const router = Router();

  router.use(requireAuth);

  router.get('/me', controller.getProfile);
  router.patch('/me', validate(updateProfileSchema), controller.updateProfile);

  return router;
}
