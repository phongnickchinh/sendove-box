import { OtaTask, OtaStatus } from '../../types/box.types';
import { IRepository } from './base.repository';

export interface IOtaRepository extends IRepository<OtaTask> {
  updateOtaStatus(taskId: string, status: OtaStatus, extras?: { progress_percent?: number; error_message?: string }): Promise<void>;
  findPendingByBoxId(boxId: string): Promise<OtaTask | null>;
}
