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

  /**
   * Pairing: User nhập mã pairing code → liên kết user với box.
   * scode bắt đầu bằng 'S', rcode bắt đầu bằng 'R'.
   */
  async pairBox(uid: string, pairingCode: string, boxName: string): Promise<{ boxId: string; role: 'sender' | 'receiver' }> {
    const isSender = pairingCode.startsWith('S');
    const codeType = isSender ? 'scode' : 'rcode';
    const role: 'sender' | 'receiver' = isSender ? 'sender' : 'receiver';

    const box = await this.boxRepo.findByPairingCode(pairingCode, codeType);
    if (!box) {
      throw new AppError(404, 'box_not_found', 'Invalid pairing code');
    }

    // Default to empty object if Firebase omitted it
    const pairing = box.pairing || {};

    // Kiểm tra slot đã bị chiếm chưa
    if (isSender && pairing.sender_id) {
      throw new AppError(400, 'slot_full', 'Sender slot is already taken');
    }
    if (!isSender && pairing.receiver_id) {
      throw new AppError(400, 'slot_full', 'Receiver slot is already taken');
    }

    // Kiểm tra user không thể vừa sender vừa receiver trên cùng 1 box
    if ((isSender && pairing.receiver_id === uid) || (!isSender && pairing.sender_id === uid)) {
      throw new AppError(400, 'conflict_role', 'You cannot be both sender and receiver for the same box');
    }

    const now = Date.now();

    // Cập nhật Box pairing
    const pairingUpdate = isSender
      ? { 'pairing/sender_id': uid, 'pairing/sender_paired_time': now }
      : { 'pairing/receiver_id': uid, 'pairing/receiver_paired_time': now };

    await this.boxRepo.update(box.id, { ...pairingUpdate, updated_at: now } as any);

    // Set p_flag để ESP32 biết có thay đổi pairing
    await this.boxRepo.updateFlags(box.id, { p_flag: true });

    // Cập nhật User boxes_list
    await this.userRepo.linkBox(uid, box.id, { role, box_name: boxName });

    return { boxId: box.id, role };
  }

  /**
   * Unpair: Ngắt kết nối user khỏi box.
   */
  async unpairBox(uid: string, boxId: string): Promise<string> {
    const box = await this.boxRepo.getById(boxId);
    if (!box) throw new AppError(404, 'box_not_found', 'Box not found');

    const pairing = box.pairing || {};

    let roleToUnpair: 'sender' | 'receiver' | null = null;
    if (pairing.sender_id === uid) roleToUnpair = 'sender';
    if (pairing.receiver_id === uid) roleToUnpair = 'receiver';

    if (!roleToUnpair) {
      throw new AppError(403, 'unauthorized', 'You are not paired to this box');
    }

    const now = Date.now();

    // Cập nhật Box
    const pairingUpdate = roleToUnpair === 'sender'
      ? { 'pairing/sender_id': null, 'pairing/sender_paired_time': null }
      : { 'pairing/receiver_id': null, 'pairing/receiver_paired_time': null };

    await this.boxRepo.update(boxId, { ...pairingUpdate, updated_at: now } as any);

    // Set p_flag
    await this.boxRepo.updateFlags(boxId, { p_flag: true });

    // Cập nhật User
    await this.userRepo.unlinkBox(uid, boxId);

    return roleToUnpair;
  }

  /**
   * Xem chi tiết box (chỉ user đã pair mới xem được)
   */
  async getBoxDetails(uid: string, boxId: string): Promise<Box> {
    const box = await this.boxRepo.getById(boxId);
    if (!box) throw new AppError(404, 'box_not_found', 'Box not found');

    const pairing = box.pairing || {};

    if (pairing.sender_id !== uid && pairing.receiver_id !== uid) {
      throw new AppError(403, 'unauthorized', 'You are not paired to this box');
    }

    return box;
  }

  /**
   * Cập nhật cấu hình WiFi cho box
   */
  async updateWifi(uid: string, boxId: string, ssid: string, pwd?: string): Promise<void> {
    await this.getBoxDetails(uid, boxId); // Validates ownership

    const now = Date.now();
    await this.boxRepo.update(boxId, {
      'config/wifi_config': { ssid, pwd: pwd || '' },
      updated_at: now,
    } as any);
  }
}
