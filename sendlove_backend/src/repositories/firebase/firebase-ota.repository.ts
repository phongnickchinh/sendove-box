import { OtaTask, OtaStatus } from '../../types/box.types';
import { FirebaseBaseRepository } from './firebase-base.repository';
import { db } from '../../firebase';

export class FirebaseOtaRepository extends FirebaseBaseRepository<OtaTask> {
  constructor() {
    super('ota_tasks');
  }

  /**
   * Cập nhật trạng thái OTA task
   */
  async updateOtaStatus(taskId: string, status: OtaStatus, extras?: { progress_percent?: number; error_message?: string }): Promise<void> {
    const updateData: Partial<OtaTask> = {
      status,
      updated_at: Date.now(),
      ...extras,
    };
    await db.ref(`${this.collectionPath}/${taskId}`).update(updateData);
  }

  /**
   * Tìm OTA task đang pending cho box cụ thể
   */
  async findPendingByBoxId(boxId: string): Promise<OtaTask | null> {
    const snapshot = await db.ref(this.collectionPath)
      .orderByChild('box_id')
      .equalTo(boxId)
      .once('value');

    if (!snapshot.exists()) return null;

    const tasks = snapshot.val();
    for (const taskId in tasks) {
      if (tasks[taskId].status === 'pending') {
        return { id: taskId, ...tasks[taskId] } as OtaTask;
      }
    }
    return null;
  }
}
