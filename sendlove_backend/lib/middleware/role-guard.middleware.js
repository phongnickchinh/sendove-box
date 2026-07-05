"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.requireRole = void 0;
const error_handler_middleware_1 = require("./error-handler.middleware");
const firebase_box_repository_1 = require("../repositories/firebase/firebase-box.repository");
const boxRepo = new firebase_box_repository_1.FirebaseBoxRepository();
const requireRole = (requiredRole) => {
    return async (req, res, next) => {
        try {
            const uid = req.user?.uid;
            const boxId = req.params.boxId;
            if (!uid)
                throw new error_handler_middleware_1.AppError(401, 'unauthorized', 'User not authenticated');
            if (!boxId)
                throw new error_handler_middleware_1.AppError(400, 'bad_request', 'Missing boxId');
            const box = await boxRepo.getById(boxId);
            if (!box)
                throw new error_handler_middleware_1.AppError(404, 'box_not_found', 'Box not found');
            if (requiredRole === 'sender' && box.pairingInfo.senderUid !== uid) {
                throw new error_handler_middleware_1.AppError(403, 'forbidden', 'Only sender can perform this action');
            }
            if (requiredRole === 'receiver' && box.pairingInfo.receiverUid !== uid) {
                throw new error_handler_middleware_1.AppError(403, 'forbidden', 'Only receiver can perform this action');
            }
            next();
        }
        catch (error) {
            next(error);
        }
    };
};
exports.requireRole = requireRole;
//# sourceMappingURL=role-guard.middleware.js.map