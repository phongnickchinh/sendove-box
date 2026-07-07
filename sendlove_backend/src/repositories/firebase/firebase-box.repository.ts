import { Box, BoxFlags, BoxStatus } from '../../types/box.types';
import { FirebaseBaseRepository } from './firebase-base.repository';
import { db } from '../../firebase';

export class FirebaseBoxRepository extends FirebaseBaseRepository<Box> {
  constructor() {
    super('boxes');
  }

  /**
   * Tìm box bằng mã pairing (scode hoặc rcode)
   */
  async findByPairingCode(code: string, codeType: 'scode' | 'rcode'): Promise<Box | null> {
    const field = `code/${codeType}`;

    const snapshot = await db.ref(this.collectionPath)
      .orderByChild(field)
      .equalTo(code)
      .limitToFirst(1)
      .once('value');

    if (!snapshot.exists()) return null;

    const data = snapshot.val();
    const boxId = Object.keys(data)[0];
    return { id: boxId, ...data[boxId] } as Box;
  }

  /**
   * Cập nhật flags (a_flag, ota_flag, p_flag)
   */
  async updateFlags(boxId: string, flags: Partial<BoxFlags>): Promise<void> {
    await db.ref(`${this.collectionPath}/${boxId}/flags`).update(flags);
  }

  /**
   * Đọc flags hiện tại
   */
  async getFlags(boxId: string): Promise<BoxFlags | null> {
    const snapshot = await db.ref(`${this.collectionPath}/${boxId}/flags`).once('value');
    if (!snapshot.exists()) return null;
    return snapshot.val() as BoxFlags;
  }

  /**
   * Cập nhật status (online, battery, charging, last_seen, fw_version)
   */
  async updateStatus(boxId: string, status: Partial<BoxStatus>): Promise<void> {
    await db.ref(`${this.collectionPath}/${boxId}/status`).update(status);
  }

  /**
   * Đọc status hiện tại
   */
  async getStatus(boxId: string): Promise<BoxStatus | null> {
    const snapshot = await db.ref(`${this.collectionPath}/${boxId}/status`).once('value');
    if (!snapshot.exists()) return null;
    return snapshot.val() as BoxStatus;
  }
}
