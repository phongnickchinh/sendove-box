"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.requireDeviceAuth = void 0;
const error_handler_middleware_1 = require("./error-handler.middleware");
const firebase_box_repository_1 = require("../repositories/firebase/firebase-box.repository");
const boxRepo = new firebase_box_repository_1.FirebaseBoxRepository();
const requireDeviceAuth = async (req, res, next) => {
    try {
        const deviceId = req.headers['x-device-id'];
        const deviceSecret = req.headers['x-device-secret'];
        if (!deviceId || !deviceSecret) {
            throw new error_handler_middleware_1.AppError(401, 'unauthorized', 'Missing device credentials');
        }
        const boxId = `box_${deviceId}`;
        const box = await boxRepo.getById(boxId);
        if (!box || box.deviceSecret !== deviceSecret) {
            throw new error_handler_middleware_1.AppError(401, 'unauthorized', 'Invalid device credentials');
        }
        req.deviceId = deviceId;
        next();
    }
    catch (error) {
        next(error);
    }
};
exports.requireDeviceAuth = requireDeviceAuth;
//# sourceMappingURL=device-auth.middleware.js.map