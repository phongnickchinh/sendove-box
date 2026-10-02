import { Router } from 'express';
import { ThemeController } from '../controllers/theme.controller';
import { requireAuth } from '../middleware/auth.middleware';
import { requireRole } from '../middleware/role-guard.middleware';

/**
 * /boxes/:boxId/theme — standby-screen theme, receiver only. The body is
 * validated in ThemeService.sanitizeWidgets (validate() can't check nested arrays).
 */
export default function themeRoutes(controller: ThemeController) {
  const router = Router({ mergeParams: true });

  router.use(requireAuth);
  router.use(requireRole('receiver'));

  router.get('/', controller.getTheme);
  router.put('/', controller.saveTheme);
  router.post('/background', controller.initiateBackground);
  router.post('/font', controller.initiateFont);

  return router;
}
