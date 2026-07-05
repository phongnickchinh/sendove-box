import { Router } from 'express';
import { MusicController } from '../controllers/music.controller';
import { requireAuth } from '../middleware/auth.middleware';

const router = Router();
const controller = new MusicController();

router.use(requireAuth);

router.get('/', controller.getMusicLibrary);
router.get('/:musicId/preview', controller.getPreviewUrl);

export default router;
