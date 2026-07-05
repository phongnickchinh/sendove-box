import { FirebaseUserRepository } from '../repositories/firebase/firebase-user.repository';
import { User } from '../types/user.types';
import { AppError } from '../middleware/error-handler.middleware';

export class UserService {
  private userRepo: FirebaseUserRepository;

  constructor() {
    this.userRepo = new FirebaseUserRepository();
  }

  async getUserProfile(uid: string): Promise<User> {
    const user = await this.userRepo.getById(uid);
    if (!user) throw new AppError(404, 'user_not_found', 'User not found');
    return user;
  }

  async updateProfile(uid: string, data: { displayName?: string, photoURL?: string }): Promise<User> {
    const user = await this.userRepo.getById(uid);
    if (!user) throw new AppError(404, 'user_not_found', 'User not found');

    const updateData: Partial<User> = {};
    if (data.displayName !== undefined) updateData.displayName = data.displayName;
    if (data.photoURL !== undefined) updateData.photoURL = data.photoURL;

    return await this.userRepo.update(uid, updateData);
  }
}
