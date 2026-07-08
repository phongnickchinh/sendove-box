import { Router } from 'express';
import { MusicController } from '../controllers/music.controller';
import { requireAuth } from '../middleware/auth.middleware';

export default function musicRoutes(controller: MusicController) {
  const router = Router();

  router.use(requireAuth);

  router.get('/', controller.getMusicLibrary);
  router.get('/:musicId/preview', controller.getPreviewUrl);

  return router;
}
