"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.default = musicRoutes;
const express_1 = require("express");
const auth_middleware_1 = require("../middleware/auth.middleware");
const role_guard_middleware_1 = require("../middleware/role-guard.middleware");
/**
 * /boxes/:boxId/music — alarm music library, receiver only (product decision).
 * Two-step upload: POST /upload returns a signed policy, POST /commit validates
 * the uploaded file and writes the DB.
 */
function musicRoutes(controller) {
    const router = (0, express_1.Router)({ mergeParams: true });
    router.use(auth_middleware_1.requireAuth);
    router.use((0, role_guard_middleware_1.requireRole)('receiver'));
    router.get('/', controller.list);
    router.post('/upload', controller.initiateUpload);
    router.post('/commit', controller.commit);
    router.patch('/:musicId', controller.rename);
    router.delete('/:musicId', controller.remove);
    router.get('/:musicId/preview', controller.previewUrl);
    return router;
}
//# sourceMappingURL=music.routes.js.map