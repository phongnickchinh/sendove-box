import { User } from '../../types/user.types';
import { FirebaseBaseRepository } from './firebase-base.repository';

export class FirebaseUserRepository extends FirebaseBaseRepository<User> {
  constructor() {
    super('users');
  }

  // Add specific user repository methods here
  async linkBox(uid: string, boxId: string, role: 'sender' | 'receiver'): Promise<void> {
    const ref = this.getRef(`${uid}/pairedBoxes/${boxId}`);
    await ref.set(role);
  }

  async unlinkBox(uid: string, boxId: string): Promise<void> {
    const ref = this.getRef(`${uid}/pairedBoxes/${boxId}`);
    await ref.remove();
  }
}
