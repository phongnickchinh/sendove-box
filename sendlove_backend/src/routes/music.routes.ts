import { Router } from 'express';
import { MusicController } from '../controllers/music.controller';
import { requireAuth } from '../middleware/auth.middleware';
import { requireRole } from '../middleware/role-guard.middleware';

/**
 * /boxes/:boxId/music — alarm music library, receiver only (product decision).
 * Two-step upload: POST /upload returns a signed policy, POST /commit validates
 * the uploaded file and writes the DB.
 */
export default function musicRoutes(controller: MusicController) {
  const router = Router({ mergeParams: true });

  router.use(requireAuth);
  router.use(requireRole('receiver'));

  router.get('/', controller.list);
  router.post('/upload', controller.initiateUpload);
  router.post('/commit', controller.commit);
  router.patch('/:musicId', controller.rename);
  router.delete('/:musicId', controller.remove);
  router.get('/:musicId/preview', controller.previewUrl);

  return router;
}
