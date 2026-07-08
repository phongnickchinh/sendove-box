import { IUserRepository } from '../repositories/interfaces/user.repository.interface';
import { FirebaseUserRepository } from '../repositories/firebase/firebase-user.repository';
import { User } from '../types/user.types';
import { AppError } from '../middleware/error-handler.middleware';

export class UserService {
  constructor(
    private userRepo: IUserRepository = new FirebaseUserRepository()
  ) {}

  async getOrCreateUserProfile(uid: string, email: string, name?: string, picture?: string): Promise<User> {
    let user = await this.userRepo.getById(uid);
    if (!user) {
      const now = Date.now();
      const newUser: User = {
        id: uid,
        email: email,
        display_name: name || email.split('@')[0],
        avatar_url: picture || null,
        is_admin: false,
        last_login_at: now,
        created_at: now,
        updated_at: now,
        boxes_list: {}
      };
      user = await this.userRepo.create(uid, newUser);
    } else {
      await this.userRepo.updateLastLogin(uid);
    }
    return user;
  }

  async updateProfile(uid: string, data: { display_name?: string; avatar_url?: string }): Promise<User> {
    const user = await this.userRepo.getById(uid);
    if (!user) throw new AppError(404, 'user_not_found', 'User not found');

    const updateData: Partial<User> = { updated_at: Date.now() };
    if (data.display_name !== undefined) updateData.display_name = data.display_name;
    if (data.avatar_url !== undefined) updateData.avatar_url = data.avatar_url;

    return await this.userRepo.update(uid, updateData);
  }
}
