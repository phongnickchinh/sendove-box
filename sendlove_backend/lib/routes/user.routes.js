"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.default = userRoutes;
const express_1 = require("express");
const auth_middleware_1 = require("../middleware/auth.middleware");
const validation_middleware_1 = require("../middleware/validation.middleware");
function userRoutes(controller) {
    const router = (0, express_1.Router)();
    router.use(auth_middleware_1.requireAuth);
    router.get('/me', controller.getProfile);
    router.patch('/me', (0, validation_middleware_1.validate)(validation_middleware_1.updateProfileSchema), controller.updateProfile);
    return router;
}
//# sourceMappingURL=user.routes.js.map