import { db } from '../../firebase';
import { IRepository } from '../interfaces/base.repository';

export class FirebaseBaseRepository<T> implements IRepository<T> {
  protected collectionPath: string;

  constructor(collectionPath: string) {
    this.collectionPath = collectionPath;
  }

  protected getRef(id: string) {
    return db.ref(`${this.collectionPath}/${id}`);
  }

  async create(id: string, data: Partial<T>): Promise<T> {
    const ref = this.getRef(id);
    await ref.set(data);
    return this.getById(id) as Promise<T>;
  }

  async getById(id: string): Promise<T | null> {
    const snapshot = await this.getRef(id).once('value');
    if (!snapshot.exists()) {
      return null;
    }
    return { ...snapshot.val(), id } as T;
  }

  async update(id: string, data: Partial<T>): Promise<T> {
    const ref = this.getRef(id);
    await ref.update(data);
    return this.getById(id) as Promise<T>;
  }

  async delete(id: string): Promise<void> {
    await this.getRef(id).remove();
  }
}
