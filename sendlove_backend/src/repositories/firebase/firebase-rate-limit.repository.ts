import { db } from '../../firebase';
import { IRateLimitRepository, RateLimitRecord } from '../interfaces/rate-limit.repository.interface';

export class FirebaseRateLimitRepository implements IRateLimitRepository {
  private basePath = 'rate_limits';

  private getKey(senderId: string, boxId: string): string {
    return `${senderId}_${boxId}`;
  }

  /**
   * Lấy record rate limit cho cặp sender + box
   */
  async get(senderId: string, boxId: string): Promise<RateLimitRecord | null> {
    const key = this.getKey(senderId, boxId);
    const snapshot = await db.ref(`${this.basePath}/${key}`).once('value');
    if (!snapshot.exists()) return null;
    return snapshot.val() as RateLimitRecord;
  }

  /**
   * Tăng count lên 1
   */
  async increment(senderId: string, boxId: string): Promise<void> {
    const key = this.getKey(senderId, boxId);
    const ref = db.ref(`${this.basePath}/${key}/count`);

    await ref.transaction((current: number | null) => {
      return (current || 0) + 1;
    });
  }

  /**
   * Reset window mới: count = 1, window_start = now
   */
  async reset(senderId: string, boxId: string): Promise<void> {
    const key = this.getKey(senderId, boxId);
    await db.ref(`${this.basePath}/${key}`).set({
      count: 1,
      window_start: Date.now(),
    });
  }

  /**
   * Atomic check-and-increment sử dụng Firebase transaction.
   * Kết hợp đọc → kiểm tra → tăng count trong 1 bước duy nhất,
   * tránh race condition khi nhiều request đồng thời.
   *
   * Trả về { allowed: true } nếu dưới limit, { allowed: false, remainingMs } nếu bị giới hạn.
   */
  async checkAndIncrement(
    senderId: string,
    boxId: string,
    maxCount: number,
    windowMs: number,
  ): Promise<{ allowed: boolean; remainingMs?: number }> {
    const key = this.getKey(senderId, boxId);
    const ref = db.ref(`${this.basePath}/${key}`);
    const now = Date.now();

    const result = await ref.transaction((current: RateLimitRecord | null) => {
      // Chưa có record → tạo mới, cho qua
      if (!current) {
        return { count: 1, window_start: now };
      }

      // Window hết hạn → reset, cho qua
      if (now - current.window_start > windowMs) {
        return { count: 1, window_start: now };
      }

      // Đã đạt giới hạn → abort transaction (return undefined)
      if (current.count >= maxCount) {
        return undefined as any;
      }

      // Dưới limit → tăng count
      return { count: current.count + 1, window_start: current.window_start };
    });

    if (!result.committed) {
      // Transaction bị abort = đã đạt rate limit
      const snapshot = await ref.once('value');
      const data = snapshot.val() as RateLimitRecord;
      const remainingMs = data ? windowMs - (now - data.window_start) : 0;
      return { allowed: false, remainingMs: Math.max(0, remainingMs) };
    }

    return { allowed: true };
  }
}
