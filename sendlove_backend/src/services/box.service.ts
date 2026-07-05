import { FirebaseBoxRepository } from '../repositories/firebase/firebase-box.repository';
import { FirebaseUserRepository } from '../repositories/firebase/firebase-user.repository';
import { Box } from '../types/box.types';
import { AppError } from '../middleware/error-handler.middleware';

export class BoxService {
  private boxRepo: FirebaseBoxRepository;
  private userRepo: FirebaseUserRepository;

  constructor() {
    this.boxRepo = new FirebaseBoxRepository();
    this.userRepo = new FirebaseUserRepository();
  }

  async pairBox(uid: string, pairingCode: string): Promise<{ boxId: string; role: 'sender' | 'receiver' }> {
    const box = await this.boxRepo.findByPairingCode(pairingCode);
    
    if (!box) {
      throw new AppError(404, 'box_not_found', 'Invalid pairing code');
    }

    const isSender = pairingCode.startsWith('SCODE');
    const role = isSender ? 'sender' : 'receiver';

    if (isSender && box.pairingInfo.senderUid) {
      throw new AppError(400, 'slot_full', 'Sender slot is already taken');
    }
    if (!isSender && box.pairingInfo.receiverUid) {
      throw new AppError(400, 'slot_full', 'Receiver slot is already taken');
    }

    // Check if the user is already paired to this box with the other role
    if ((isSender && box.pairingInfo.receiverUid === uid) || (!isSender && box.pairingInfo.senderUid === uid)) {
      throw new AppError(400, 'conflict_role', 'You cannot be both sender and receiver for the same box');
    }

    // Update Box
    const updateData = isSender ? { 'pairingInfo/senderUid': uid } : { 'pairingInfo/receiverUid': uid };
    await this.boxRepo.update(box.boxId, updateData as any);

    // Update User
    await this.userRepo.linkBox(uid, box.boxId, role);

    return { boxId: box.boxId, role };
  }

  async unpairBox(uid: string, boxId: string): Promise<string> {
    const box = await this.boxRepo.getById(boxId);
    if (!box) throw new AppError(404, 'box_not_found', 'Box not found');

    let roleToUnpair: 'sender' | 'receiver' | null = null;
    if (box.pairingInfo.senderUid === uid) roleToUnpair = 'sender';
    if (box.pairingInfo.receiverUid === uid) roleToUnpair = 'receiver';

    if (!roleToUnpair) {
      throw new AppError(403, 'unauthorized', 'You are not paired to this box');
    }

    // Update Box
    const updateData = roleToUnpair === 'sender' ? { 'pairingInfo/senderUid': null } : { 'pairingInfo/receiverUid': null };
    await this.boxRepo.update(boxId, updateData as any);

    // Update User
    await this.userRepo.unlinkBox(uid, boxId);

    return roleToUnpair;
  }

  async getBoxDetails(uid: string, boxId: string): Promise<Box> {
    const box = await this.boxRepo.getById(boxId);
    if (!box) throw new AppError(404, 'box_not_found', 'Box not found');

    if (box.pairingInfo.senderUid !== uid && box.pairingInfo.receiverUid !== uid) {
      throw new AppError(403, 'unauthorized', 'You are not paired to this box');
    }

    return box;
  }

  async updateWifi(uid: string, boxId: string, ssid: string, password?: string): Promise<void> {
    await this.getBoxDetails(uid, boxId); // Validates ownership implicitly

    await this.boxRepo.update(boxId, {
      wifiConfig: {
        ssid,
        password,
        status: 'pending_apply'
      }
    });

    // Notify ESP32 via polling cache
    await this.boxRepo.updatePollingCache(boxId, { hasNewWifiConfig: true });
  }
}
