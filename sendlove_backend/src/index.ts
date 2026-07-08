import * as functions from 'firebase-functions';
import express from 'express';
import cors from 'cors';
import helmet from 'helmet';
import { errorHandler } from './middleware/error-handler.middleware';

// Initialize Firebase App
import './firebase';

// Import routes
import authRoutes from './routes/auth.routes';
import userRoutes from './routes/user.routes';
import boxRoutes from './routes/box.routes';
import deviceRoutes from './routes/device.routes';
import musicRoutes from './routes/music.routes';
import messageRoutes from './routes/message.routes';
import alarmRoutes from './routes/alarm.routes';

// Import DI Container
import { createContainer } from './di/container';

const app = express();

// Initialize DI Container
const container = createContainer();

// Middleware
// Security headers (X-Content-Type-Options, X-Frame-Options, Strict-Transport-Security, etc.)
app.use(helmet());

// TODO: Restrict CORS origins before production deployment.
// Current config allows all origins for development/testing convenience.
// Example: app.use(cors({ origin: ['https://iot-app-839a2.web.app'] }));
app.use(cors({ origin: true }));

// Parse JSON with explicit body size limit to prevent DoS via large payloads
app.use(express.json({ limit: '10kb' }));

// Basic health check
app.get('/health', (req, res) => {
  res.status(200).json({ success: true, message: 'Sendove Box API is running' });
});

// Setup Routes with Injected Controllers
app.use('/auth', authRoutes(container.authController));
app.use('/users', userRoutes(container.userController));

const msgRouter = messageRoutes(container.messageController);
const alrmRouter = alarmRoutes(container.alarmController);
app.use('/boxes', boxRoutes(container.boxController, msgRouter, alrmRouter));

app.use('/device', deviceRoutes(container.deviceController));
app.use('/music', musicRoutes(container.musicController));

// Error Handling Middleware (must be the last middleware)
app.use(errorHandler);

// Export the API as a Firebase Cloud Function
export const api = functions.https.onRequest(app);
