import { IUserRepository } from '../interfaces/user.repository.interface';

export class MockUserRepository implements IUserRepository {
  getById = jest.fn();
  getAll = jest.fn();
  create = jest.fn();
  update = jest.fn();
  delete = jest.fn();
  linkBox = jest.fn();
  unlinkBox = jest.fn();
  updateLastLogin = jest.fn();
  softDelete = jest.fn();
}
