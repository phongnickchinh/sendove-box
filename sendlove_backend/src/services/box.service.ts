import { IBoxRepository } from '../repositories/interfaces/box.repository.interface';
import { IUserRepository } from '../repositories/interfaces/user.repository.interface';
import { FirebaseBoxRepository } from '../repositories/firebase/firebase-box.repository';
import { FirebaseUserRepository } from '../repositories/firebase/firebase-user.repository';
import { AppError } from '../middleware/error-handler.middleware';
// Import the submodule; do NOT `import * as admin` and use `admin.database.ServerValue`:
// TypeScript compiles that to __importStar, and firebase-admin 12 doesn't expose
// `database` as an own key of the module -> undefined at runtime ("reading 'increment'").
import { ServerValue } from 'firebase-admin/database';

export class BoxService {
  constructor(
    private boxRepo: IBoxRepository = new FirebaseBoxRepository(),
    private userRepo: IUserRepository = new FirebaseUserRepository()
  ) {}

  /**
   * Pairing: the user enters a pairing code → the user is linked to the box.
   * scode starts with 'S', rcode with 'R'.
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

    // The slot must be free
    if (isSender && pairing.sender_id) {
      throw new AppError(400, 'slot_full', 'Sender slot is already taken');
    }
    if (!isSender && pairing.receiver_id) {
      throw new AppError(400, 'slot_full', 'Receiver slot is already taken');
    }

    // A user can't be both sender and receiver of the same box
    const isAlreadyReceiver = isSender && (pairing.receiver_id === uid);
    const isAlreadySender = !isSender && (pairing.sender_id === uid);
    if (isAlreadyReceiver || isAlreadySender) {
      throw new AppError(400, 'conflict_role', 'You cannot be both sender and receiver for the same box');
    }

    const now = Date.now();

    // Update the box's pairing
    const pairingUpdate = isSender
      ? { 'pairing/sender_id': uid, 'pairing/sender_paired_time': now }
      : { 'pairing/receiver_id': uid, 'pairing/receiver_paired_time': now };

    await this.boxRepo.update(box.id, { ...pairingUpdate, updated_at: now } as any);

    // Set p_flag so the ESP32 knows the pairing changed
    await this.boxRepo.updateFlags(box.id, { p_flag: true });

    // Update the user's boxes_list
    await this.userRepo.linkBox(uid, box.id, { role, box_name: boxName });

    return { boxId: box.id, role };
  }

  /** Unpair: detach the user from the box. */
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

    // Update the box
    const pairingUpdate = roleToUnpair === 'sender'
      ? { 'pairing/sender_id': null, 'pairing/sender_paired_time': null }
      : { 'pairing/receiver_id': null, 'pairing/receiver_paired_time': null };

    await this.boxRepo.update(boxId, { ...pairingUpdate, updated_at: now } as any);

    // Set p_flag
    await this.boxRepo.updateFlags(boxId, { p_flag: true });

    // Update the user
    await this.userRepo.unlinkBox(uid, boxId);

    return roleToUnpair;
  }

  /**
   * Box details (paired users only).
   * device_secret and the Wi-Fi password are stripped before returning.
   */
  async getBoxDetails(uid: string, boxId: string) {
    const box = await this.boxRepo.getById(boxId);
    if (!box) throw new AppError(404, 'box_not_found', 'Box not found');

    const pairing = box.pairing || {};

    if (pairing.sender_id !== uid && pairing.receiver_id !== uid) {
      throw new AppError(403, 'unauthorized', 'You are not paired to this box');
    }

    // Sanitize: strip sensitive fields before returning from the API
    const { device_secret, config, ...rest } = box;
    const safeConfig = config ? {
      ...config,
      wifi_config: config.wifi_config
        ? { ssid: config.wifi_config.ssid }
        : undefined,
    } : undefined;

    return { ...rest, config: safeConfig };
  }

  /**
   * Update the box's Wi-Fi config.
   * pwd undefined = the user didn't touch the password field → KEEP the stored
   * password and change only the ssid. (The web can never read the password
   * back, so it can't resend it.)
   * pwd "" = an open network, chosen explicitly.
   */
  async updateWifi(uid: string, boxId: string, ssid: string, pwd?: string): Promise<void> {
    await this.getBoxDetails(uid, boxId); // Validates ownership

    const updates: Record<string, unknown> = { updated_at: Date.now() };
    if (pwd === undefined) {
      updates['config/wifi_config/ssid'] = ssid;
    } else {
      updates['config/wifi_config'] = { ssid, pwd };
    }
    await this.boxRepo.update(boxId, updates as any);
  }

  /**
   * Update the box's led_state / display_brightness / playback_volume.
   * Only the fields passed in are overwritten.
   */
  async updateBoxConfig(uid: string, boxId: string, data: {
    led_state?: string;
    display_brightness?: number;
    playback_volume?: number;
  }): Promise<void> {
    await this.getBoxDetails(uid, boxId); // Validates ownership

    const updates: Record<string, any> = { updated_at: Date.now() };
    if (data.led_state !== undefined) updates['config/led_state'] = data.led_state;
    if (data.display_brightness !== undefined) updates['config/display_brightness'] = data.display_brightness;
    if (data.playback_volume !== undefined) updates['config/playback_volume'] = data.playback_volume;
    // The box writes this number to status/config_rev once applied -> the web
    // compares the two to show "applied" or "waiting for the box". Incremented
    // atomically on the server, so two close saves never share a rev.
    updates['config/config_rev'] = ServerValue.increment(1);

    await this.boxRepo.update(boxId, updates as any);

    // Set config_flag so the ESP32 re-reads its config
    await this.boxRepo.updateFlags(boxId, { config_flag: true });
  }
}
