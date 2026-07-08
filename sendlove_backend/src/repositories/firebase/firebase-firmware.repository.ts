import { Firmware } from '../../types/box.types';
import { FirebaseBaseRepository } from './firebase-base.repository';
import { IFirmwareRepository } from '../interfaces/firmware.repository.interface';

export class FirebaseFirmwareRepository extends FirebaseBaseRepository<Firmware> implements IFirmwareRepository {
  constructor() {
    super('firmware');
  }

  /**
   * Lấy firmware mới nhất (version cao nhất)
   */
  async getLatest(): Promise<Firmware | null> {
    const { db } = await import('../../firebase');
    const snapshot = await db.ref(this.collectionPath)
      .orderByChild('created_at')
      .limitToLast(1)
      .once('value');

    if (!snapshot.exists()) return null;

    const data = snapshot.val();
    const fwId = Object.keys(data)[0];
    return data[fwId] as Firmware;
  }
}
