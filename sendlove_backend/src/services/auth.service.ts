import { FirebaseUserRepository } from '../repositories/firebase/firebase-user.repository';
import { User } from '../types/user.types';
import { AppError } from '../middleware/error-handler.middleware';

export class AuthService {
  private userRepo: FirebaseUserRepository;

  constructor() {
    this.userRepo = new FirebaseUserRepository();
  }

  /**
   * Handle user login via Google OAuth token validation.
   * In a real implementation, you might exchange a custom token or just ensure the user exists in RTDB.
   */
  async handleGoogleLogin(decodedToken: any): Promise<User> {
    const { uid, email, name, picture } = decodedToken;

    let user = await this.userRepo.getById(uid);

    if (!user) {
      // Create new user
      user = await this.userRepo.create(uid, {
        uid,
        email: email || null,
        displayName: name || null,
        photoURL: picture || null,
        createdAt: Date.now(),
      });
    } else {
      // Update profile info if changed
      let changed = false;
      if (name && user.displayName !== name) {
        user.displayName = name;
        changed = true;
      }
      if (picture && user.photoURL !== picture) {
        user.photoURL = picture;
        changed = true;
      }

      if (changed) {
        user = await this.userRepo.update(uid, {
          displayName: user.displayName,
          photoURL: user.photoURL
        });
      }
    }

    return user;
  }

  async deleteAccount(uid: string): Promise<string[]> {
    const user = await this.userRepo.getById(uid);
    if (!user) throw new AppError(404, 'user_not_found', 'User not found');

    const deletedBoxes: string[] = [];

    // Unpair from all boxes
    if (user.pairedBoxes) {
      for (const [boxId] of Object.entries(user.pairedBoxes)) {
        // We just return the list of boxes here, 
        // the BoxService should ideally handle the actual unpairing logic on the Box node
        deletedBoxes.push(boxId);
      }
    }

    // Delete user from RTDB
    await this.userRepo.delete(uid);

    return deletedBoxes;
  }
}
