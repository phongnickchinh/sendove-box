"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.default = musicRoutes;
const express_1 = require("express");
const auth_middleware_1 = require("../middleware/auth.middleware");
function musicRoutes(controller) {
    const router = (0, express_1.Router)();
    router.use(auth_middleware_1.requireAuth);
    router.get('/', controller.getMusicLibrary);
    router.get('/:musicId/preview', controller.getPreviewUrl);
    return router;
}
//# sourceMappingURL=music.routes.js.map