import { User, UserBoxEntry } from '../../types/user.types';
import { FirebaseBaseRepository } from './firebase-base.repository';

export class FirebaseUserRepository extends FirebaseBaseRepository<User> {
  constructor() {
    super('users');
  }

  /**
   * Liên kết user với box (ghi vào boxes_list)
   */
  async linkBox(uid: string, boxId: string, entry: UserBoxEntry): Promise<void> {
    const ref = this.getRef(`${uid}/boxes_list/${boxId}`);
    await ref.set(entry);
  }

  /**
   * Ngắt liên kết user với box
   */
  async unlinkBox(uid: string, boxId: string): Promise<void> {
    const ref = this.getRef(`${uid}/boxes_list/${boxId}`);
    await ref.remove();
  }

  /**
   * Cập nhật last_login_at
   */
  async updateLastLogin(uid: string): Promise<void> {
    await this.getRef(`${uid}/last_login_at`).set(Date.now());
  }
}
