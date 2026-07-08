import * as admin from 'firebase-admin';
import { IUserRepository } from '../repositories/interfaces/user.repository.interface';
import { IBoxRepository } from '../repositories/interfaces/box.repository.interface';
import { FirebaseUserRepository } from '../repositories/firebase/firebase-user.repository';
import { FirebaseBoxRepository } from '../repositories/firebase/firebase-box.repository';
import { User } from '../types/user.types';
import { AppError } from '../middleware/error-handler.middleware';
import { db } from '../firebase';

export class AuthService {
  constructor(
    private userRepo: IUserRepository = new FirebaseUserRepository(),
    private boxRepo: IBoxRepository = new FirebaseBoxRepository()
  ) {}

  /**
   * Xử lý đăng nhập Google OAuth.
   * Tạo user mới nếu chưa tồn tại, cập nhật profile nếu đã có.
   */
  async handleGoogleLogin(decodedToken: any): Promise<User> {
    const { uid, email, name, picture } = decodedToken;
    const now = Date.now();

    let user = await this.userRepo.getById(uid);

    if (!user) {
      // Tạo user mới
      user = await this.userRepo.create(uid, {
        id: uid,
        email: email || '',
        display_name: name || '',
        is_admin: false,
        avatar_url: picture || null,
        last_login_at: now,
        boxes_list: {},
        created_at: now,
        updated_at: now,
      });
    } else {
      // Cập nhật profile nếu thay đổi + ghi last_login_at
      const updateData: Partial<User> = { last_login_at: now, updated_at: now };

      if (name && user.display_name !== name) updateData.display_name = name;
      if (picture && user.avatar_url !== picture) updateData.avatar_url = picture;

      user = await this.userRepo.update(uid, updateData);
    }

    return user;
  }

  async deleteAccount(uid: string): Promise<string[]> {
    const user = await this.userRepo.getById(uid);
    if (!user) throw new AppError(404, 'user_not_found', 'User not found');

    const deletedBoxes: string[] = [];

    // 1. Hard delete: Unpair từ tất cả các box (xoá dữ liệu nhân bản)
    if (user.boxes_list) {
      for (const boxId of Object.keys(user.boxes_list)) {
        try {
          const box = await this.boxRepo.getById(boxId);
          if (box) {
            const pairing = box.pairing || {};
            const now = Date.now();

            if (pairing.sender_id === uid) {
              await this.boxRepo.update(boxId, {
                'pairing/sender_id': null,
                'pairing/sender_paired_time': null,
                updated_at: now,
              } as any);
            }
            if (pairing.receiver_id === uid) {
              await this.boxRepo.update(boxId, {
                'pairing/receiver_id': null,
                'pairing/receiver_paired_time': null,
                updated_at: now,
              } as any);
            }

            // Thông báo device có thay đổi pairing
            await this.boxRepo.updateFlags(boxId, { p_flag: true });
          }
        } catch (error) {
          console.warn(`[AuthService] Failed to unpair box ${boxId}:`, error);
        }
        deletedBoxes.push(boxId);
      }
    }

    // 2. Hard delete: Xoá rate limit records cho user này
    try {
      const rateLimitsSnapshot = await db.ref('rate_limits')
        .orderByKey()
        .startAt(`${uid}_`)
        .endAt(`${uid}_\uffff`)
        .once('value');

      if (rateLimitsSnapshot.exists()) {
        const updates: Record<string, null> = {};
        rateLimitsSnapshot.forEach((child) => {
          updates[`rate_limits/${child.key}`] = null;
          return false;
        });
        await db.ref().update(updates);
      }
    } catch (error) {
      console.warn(`[AuthService] Failed to clean rate limits for ${uid}:`, error);
    }

    // 3. Soft delete: Đánh dấu user đã xoá (giữ data cho audit/history)
    await this.userRepo.softDelete(uid);

    // 4. Hard delete: Xoá Firebase Auth user (ngăn đăng nhập lại)
    try {
      await admin.auth().deleteUser(uid);
    } catch (error) {
      console.warn(`[AuthService] Failed to delete Firebase Auth user ${uid}:`, error);
    }

    return deletedBoxes;
  }
}
