import { Box, BoxFlags, BoxStatus } from '../../types/box.types';
import { IRepository } from './base.repository';

export interface IBoxRepository extends IRepository<Box> {
  findByPairingCode(code: string, codeType: 'scode' | 'rcode'): Promise<Box | null>;
  updateFlags(boxId: string, flags: Partial<BoxFlags>): Promise<void>;
  getFlags(boxId: string): Promise<BoxFlags | null>;
  updateStatus(boxId: string, status: Partial<BoxStatus>): Promise<void>;
  getStatus(boxId: string): Promise<BoxStatus | null>;
}
