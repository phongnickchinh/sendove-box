import * as functions from 'firebase-functions';
import express from 'express';
import cors from 'cors';
import { errorHandler } from './middleware/error-handler.middleware';

// Initialize Firebase App
import './firebase';

// Import routes
import authRoutes from './routes/auth.routes';
import userRoutes from './routes/user.routes';
import boxRoutes from './routes/box.routes';
import deviceRoutes from './routes/device.routes';
import musicRoutes from './routes/music.routes';

const app = express();

// Middleware
app.use(cors({ origin: true }));
app.use(express.json());

// Basic health check
app.get('/health', (req, res) => {
  res.status(200).json({ success: true, message: 'Sendove Box API is running' });
});

// Setup Routes
app.use('/auth', authRoutes);
app.use('/users', userRoutes);
app.use('/boxes', boxRoutes);
app.use('/device', deviceRoutes);
app.use('/music', musicRoutes);

// Error Handling Middleware (must be the last middleware)
app.use(errorHandler);

// Export the API as a Firebase Cloud Function
export const api = functions.https.onRequest(app);
