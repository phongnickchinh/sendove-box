"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.default = musicRoutes;
const express_1 = require("express");
const auth_middleware_1 = require("../middleware/auth.middleware");
const role_guard_middleware_1 = require("../middleware/role-guard.middleware");
/**
 * /boxes/:boxId/music — thư viện nhạc báo thức của hộp. Chỉ người nhận (user chốt
 * 2026-09-24), giống báo thức và theme. Tải lên 2 bước: POST /upload lấy signed POST
 * policy -> web tải thẳng lên Storage -> POST /commit để backend kiểm file và ghi DB.
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