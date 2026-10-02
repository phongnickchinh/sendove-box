import { NextFunction, Response } from 'express';
import { AuthenticatedRequest, ApiResponse } from '../types/api.types';
import { AuthService } from '../services/auth.service';

export class AuthController {
  constructor(
    private authService: AuthService = new AuthService()
  ) {}

  // Not used directly if relying on Firebase Auth SDK on frontend, 
  // but useful if you want to sync users explicitly
  public login = async (req: AuthenticatedRequest, res: Response<ApiResponse>, next: NextFunction) => {
    try {
      // req.user is set by the auth middleware
      res.status(200).json({ success: true, data: { message: 'Logged in' } });
    } catch (error) {
      next(error);
    }
  };

  public deleteAccount = async (req: AuthenticatedRequest, res: Response<ApiResponse>, next: NextFunction) => {
    try {
      const uid = req.user!.uid;
      const deletedBoxes = await this.authService.deleteAccount(uid);
      res.status(200).json({
        success: true,
        data: { deletedBoxes, message: 'Account permanently deleted' }
      });
    } catch (error) {
      next(error);
    }
  };
}
