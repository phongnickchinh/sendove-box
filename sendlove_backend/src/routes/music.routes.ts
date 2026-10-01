import { Router } from 'express';
import { MusicController } from '../controllers/music.controller';
import { requireAuth } from '../middleware/auth.middleware';
import { requireRole } from '../middleware/role-guard.middleware';

/**
 * /boxes/:boxId/music — the box's alarm music library. Receiver only (product
 * decision), like alarms and themes. Two-step upload: POST /upload returns a
 * signed POST policy → the web uploads straight to Storage → POST /commit has
 * the backend validate the file and write the DB.
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
