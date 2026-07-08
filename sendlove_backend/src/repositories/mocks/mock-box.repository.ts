import { IBoxRepository } from '../interfaces/box.repository.interface';

export class MockBoxRepository implements IBoxRepository {
  getById = jest.fn();
  getAll = jest.fn();
  create = jest.fn();
  update = jest.fn();
  delete = jest.fn();
  findByPairingCode = jest.fn();
  updateFlags = jest.fn();
  getFlags = jest.fn();
  updateStatus = jest.fn();
  getStatus = jest.fn();
}
