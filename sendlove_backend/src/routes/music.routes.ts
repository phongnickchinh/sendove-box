import { Router } from 'express';
import { MusicController } from '../controllers/music.controller';
import { requireAuth } from '../middleware/auth.middleware';
import { requireRole } from '../middleware/role-guard.middleware';

/**
 * /boxes/:boxId/music — thư viện nhạc báo thức của hộp. Chỉ người nhận (user chốt
 * 2026-09-24), giống báo thức và theme. Tải lên 2 bước: POST /upload lấy signed POST
 * policy -> web tải thẳng lên Storage -> POST /commit để backend kiểm file và ghi DB.
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
