import { User, UserBoxEntry } from '../../types/user.types';
import { FirebaseBaseRepository } from './firebase-base.repository';
import { IUserRepository } from '../interfaces/user.repository.interface';

export class FirebaseUserRepository extends FirebaseBaseRepository<User> implements IUserRepository {
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

  /**
   * Xoá mềm user: set is_deleted = true và deleted_at.
   * Dữ liệu user vẫn được giữ lại cho mục đích audit/history.
   */
  async softDelete(uid: string): Promise<void> {
    const now = Date.now();
    await this.getRef(uid).update({
      is_deleted: true,
      deleted_at: now,
      updated_at: now,
    });
  }
}
