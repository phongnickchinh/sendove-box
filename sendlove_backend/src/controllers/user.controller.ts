import { NextFunction, Response } from 'express';
import { AuthenticatedRequest, ApiResponse } from '../types/api.types';
import { UserService } from '../services/user.service';

export class UserController {
  private userService: UserService;

  constructor() {
    this.userService = new UserService();
  }

  public getProfile = async (req: AuthenticatedRequest, res: Response<ApiResponse>, next: NextFunction) => {
    try {
      const uid = req.user!.uid;
      const user = await this.userService.getUserProfile(uid);
      res.status(200).json({ success: true, data: user });
    } catch (error) {
      next(error);
    }
  };

  public updateProfile = async (req: AuthenticatedRequest, res: Response<ApiResponse>, next: NextFunction) => {
    try {
      const uid = req.user!.uid;
      const user = await this.userService.updateProfile(uid, req.body);
      res.status(200).json({ success: true, data: user });
    } catch (error) {
      next(error);
    }
  };
}
