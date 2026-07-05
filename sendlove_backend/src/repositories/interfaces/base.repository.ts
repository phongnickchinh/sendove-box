export interface IRepository<T> {
  create(id: string, data: Partial<T>): Promise<T>;
  getById(id: string): Promise<T | null>;
  update(id: string, data: Partial<T>): Promise<T>;
  delete(id: string): Promise<void>;
}
