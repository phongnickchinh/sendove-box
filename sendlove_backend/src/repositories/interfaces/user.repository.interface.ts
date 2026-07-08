import { User, UserBoxEntry } from '../../types/user.types';
import { IRepository } from './base.repository';

export interface IUserRepository extends IRepository<User> {
  linkBox(uid: string, boxId: string, entry: UserBoxEntry): Promise<void>;
  unlinkBox(uid: string, boxId: string): Promise<void>;
  updateLastLogin(uid: string): Promise<void>;
  softDelete(uid: string): Promise<void>;
}
