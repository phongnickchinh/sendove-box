"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.default = themeRoutes;
const express_1 = require("express");
const auth_middleware_1 = require("../middleware/auth.middleware");
const role_guard_middleware_1 = require("../middleware/role-guard.middleware");
/**
 * /boxes/:boxId/theme — giao diện màn chờ. Chỉ người nhận (người giữ hộp) sửa
 * được, giống báo thức. Body được kiểm tra trong ThemeService.sanitizeWidgets
 * (mảng lồng nhau, validate() phẳng không kiểm được).
 */
function themeRoutes(controller) {
    const router = (0, express_1.Router)({ mergeParams: true });
    router.use(auth_middleware_1.requireAuth);
    router.use((0, role_guard_middleware_1.requireRole)('receiver'));
    router.get('/', controller.getTheme);
    router.put('/', controller.saveTheme);
    router.post('/background', controller.initiateBackground);
    router.post('/font', controller.initiateFont);
    return router;
}
//# sourceMappingURL=theme.routes.js.map