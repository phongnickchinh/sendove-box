import { Firmware } from '../../types/box.types';
import { IRepository } from './base.repository';

export interface IFirmwareRepository extends IRepository<Firmware> {
  getLatest(): Promise<Firmware | null>;
}
