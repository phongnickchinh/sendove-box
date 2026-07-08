"use strict";
var __createBinding = (this && this.__createBinding) || (Object.create ? (function(o, m, k, k2) {
    if (k2 === undefined) k2 = k;
    var desc = Object.getOwnPropertyDescriptor(m, k);
    if (!desc || ("get" in desc ? !m.__esModule : desc.writable || desc.configurable)) {
      desc = { enumerable: true, get: function() { return m[k]; } };
    }
    Object.defineProperty(o, k2, desc);
}) : (function(o, m, k, k2) {
    if (k2 === undefined) k2 = k;
    o[k2] = m[k];
}));
var __setModuleDefault = (this && this.__setModuleDefault) || (Object.create ? (function(o, v) {
    Object.defineProperty(o, "default", { enumerable: true, value: v });
}) : function(o, v) {
    o["default"] = v;
});
var __importStar = (this && this.__importStar) || (function () {
    var ownKeys = function(o) {
        ownKeys = Object.getOwnPropertyNames || function (o) {
            var ar = [];
            for (var k in o) if (Object.prototype.hasOwnProperty.call(o, k)) ar[ar.length] = k;
            return ar;
        };
        return ownKeys(o);
    };
    return function (mod) {
        if (mod && mod.__esModule) return mod;
        var result = {};
        if (mod != null) for (var k = ownKeys(mod), i = 0; i < k.length; i++) if (k[i] !== "default") __createBinding(result, mod, k[i]);
        __setModuleDefault(result, mod);
        return result;
    };
})();
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
exports.api = void 0;
const functions = __importStar(require("firebase-functions"));
const express_1 = __importDefault(require("express"));
const cors_1 = __importDefault(require("cors"));
const helmet_1 = __importDefault(require("helmet"));
const error_handler_middleware_1 = require("./middleware/error-handler.middleware");
// Initialize Firebase App
require("./firebase");
// Import routes
const auth_routes_1 = __importDefault(require("./routes/auth.routes"));
const user_routes_1 = __importDefault(require("./routes/user.routes"));
const box_routes_1 = __importDefault(require("./routes/box.routes"));
const device_routes_1 = __importDefault(require("./routes/device.routes"));
const music_routes_1 = __importDefault(require("./routes/music.routes"));
const message_routes_1 = __importDefault(require("./routes/message.routes"));
const alarm_routes_1 = __importDefault(require("./routes/alarm.routes"));
// Import DI Container
const container_1 = require("./di/container");
const app = (0, express_1.default)();
// Initialize DI Container
const container = (0, container_1.createContainer)();
// Middleware
// Security headers (X-Content-Type-Options, X-Frame-Options, Strict-Transport-Security, etc.)
app.use((0, helmet_1.default)());
// TODO: Restrict CORS origins before production deployment.
// Current config allows all origins for development/testing convenience.
// Example: app.use(cors({ origin: ['https://iot-app-839a2.web.app'] }));
app.use((0, cors_1.default)({ origin: true }));
// Parse JSON with explicit body size limit to prevent DoS via large payloads
app.use(express_1.default.json({ limit: '10kb' }));
// Basic health check
app.get('/health', (req, res) => {
    res.status(200).json({ success: true, message: 'Sendove Box API is running' });
});
// Setup Routes with Injected Controllers
app.use('/auth', (0, auth_routes_1.default)(container.authController));
app.use('/users', (0, user_routes_1.default)(container.userController));
const msgRouter = (0, message_routes_1.default)(container.messageController);
const alrmRouter = (0, alarm_routes_1.default)(container.alarmController);
app.use('/boxes', (0, box_routes_1.default)(container.boxController, msgRouter, alrmRouter));
app.use('/device', (0, device_routes_1.default)(container.deviceController));
app.use('/music', (0, music_routes_1.default)(container.musicController));
// Error Handling Middleware (must be the last middleware)
app.use(error_handler_middleware_1.errorHandler);
// Export the API as a Firebase Cloud Function
exports.api = functions.https.onRequest(app);
//# sourceMappingURL=index.js.map