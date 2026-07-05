import { Box, DevicePollingCache } from '../../types/box.types';
import { FirebaseBaseRepository } from './firebase-base.repository';
import { db } from '../../firebase';

export class FirebaseBoxRepository extends FirebaseBaseRepository<Box> {
  constructor() {
    super('boxes');
  }

  // Update polling cache for a box
  async updatePollingCache(boxId: string, data: Partial<DevicePollingCache>): Promise<void> {
    const ref = db.ref(`device_polling_cache/${boxId}`);
    await ref.update(data);
  }

  // Get polling cache for a box
  async getPollingCache(boxId: string): Promise<DevicePollingCache | null> {
    const snapshot = await db.ref(`device_polling_cache/${boxId}`).once('value');
    if (!snapshot.exists()) return null;
    return snapshot.val() as DevicePollingCache;
  }

  // Find box by pairing code (scode or rcode)
  async findByPairingCode(code: string): Promise<Box | null> {
    const isSender = code.startsWith('SCODE');
    const field = isSender ? 'pairingInfo/senderCode' : 'pairingInfo/receiverCode';
    
    const snapshot = await db.ref(this.collectionPath)
      .orderByChild(field)
      .equalTo(code)
      .limitToFirst(1)
      .once('value');
      
    if (!snapshot.exists()) return null;
    
    const boxes = snapshot.val();
    const boxId = Object.keys(boxes)[0];
    return { boxId, ...boxes[boxId] } as Box;
  }
}
