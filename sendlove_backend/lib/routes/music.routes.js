"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
const express_1 = require("express");
const music_controller_1 = require("../controllers/music.controller");
const auth_middleware_1 = require("../middleware/auth.middleware");
const router = (0, express_1.Router)();
const controller = new music_controller_1.MusicController();
router.use(auth_middleware_1.requireAuth);
router.get('/', controller.getMusicLibrary);
router.get('/:musicId/preview', controller.getPreviewUrl);
exports.default = router;
//# sourceMappingURL=music.routes.js.map