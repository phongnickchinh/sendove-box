import { User, UserBoxEntry } from '../../types/user.types';
import { FirebaseBaseRepository } from './firebase-base.repository';
import { IUserRepository } from '../interfaces/user.repository.interface';

export class FirebaseUserRepository extends FirebaseBaseRepository<User> implements IUserRepository {
  constructor() {
    super('users');
  }

  /** Link a user to a box (writes boxes_list). */
  async linkBox(uid: string, boxId: string, entry: UserBoxEntry): Promise<void> {
    const ref = this.getRef(`${uid}/boxes_list/${boxId}`);
    await ref.set(entry);
  }

  /** Unlink a user from a box. */
  async unlinkBox(uid: string, boxId: string): Promise<void> {
    const ref = this.getRef(`${uid}/boxes_list/${boxId}`);
    await ref.remove();
  }

  /** Update last_login_at. */
  async updateLastLogin(uid: string): Promise<void> {
    await this.getRef(`${uid}/last_login_at`).set(Date.now());
  }

  /**
   * Soft-delete a user: sets is_deleted = true and deleted_at.
   * The user's data is kept for audit/history.
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
