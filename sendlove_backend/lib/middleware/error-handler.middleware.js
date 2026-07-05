"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.AppError = exports.errorHandler = void 0;
const errorHandler = (err, req, res, next) => {
    console.error('[Error Handler]', err);
    const statusCode = err.statusCode || 500;
    const message = err.message || 'Internal Server Error';
    const code = err.code || 'internal_error';
    res.status(statusCode).json({
        success: false,
        error: {
            code,
            message,
        }
    });
};
exports.errorHandler = errorHandler;
class AppError extends Error {
    constructor(statusCode, code, message) {
        super(message);
        this.statusCode = statusCode;
        this.code = code;
    }
}
exports.AppError = AppError;
//# sourceMappingURL=error-handler.middleware.js.map