import { Router } from 'express';
import { ThemeController } from '../controllers/theme.controller';
import { requireAuth } from '../middleware/auth.middleware';
import { requireRole } from '../middleware/role-guard.middleware';

/**
 * /boxes/:boxId/theme — giao diện màn chờ. Chỉ người nhận (người giữ hộp) sửa
 * được, giống báo thức. Body được kiểm tra trong ThemeService.sanitizeWidgets
 * (mảng lồng nhau, validate() phẳng không kiểm được).
 */
export default function themeRoutes(controller: ThemeController) {
  const router = Router({ mergeParams: true });

  router.use(requireAuth);
  router.use(requireRole('receiver'));

  router.get('/', controller.getTheme);
  router.put('/', controller.saveTheme);
  router.post('/background', controller.initiateBackground);

  return router;
}
