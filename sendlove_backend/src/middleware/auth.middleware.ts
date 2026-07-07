import { Response, NextFunction } from 'express';
import * as admin from 'firebase-admin';
import { AuthenticatedRequest, ApiResponse } from '../types/api.types';
import { AppError } from './error-handler.middleware';

export const requireAuth = async (req: AuthenticatedRequest, res: Response<ApiResponse>, next: NextFunction) => {
  try {
    const authHeader = req.headers.authorization;
    if (!authHeader || !authHeader.startsWith('Bearer ')) {
      throw new AppError(401, 'unauthorized', 'Missing or invalid Authorization header');
    }

    const idToken = authHeader.split('Bearer ')[1];
    
    // In local development/testing, you might mock this or skip verification
    // For now, we use standard Firebase Admin verification
    const decodedToken = await admin.auth().verifyIdToken(idToken);
    
    req.user = {
      uid: decodedToken.uid,
      email: decodedToken.email,
    } as any;
    (req.user as any).name = decodedToken.name;
    (req.user as any).picture = decodedToken.picture;
    
    next();
  } catch (error) {
    next(new AppError(401, 'unauthorized', 'Invalid or expired token'));
  }
};
