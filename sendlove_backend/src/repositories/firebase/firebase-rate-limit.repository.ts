import { db } from '../../firebase';
import { IRateLimitRepository, RateLimitRecord } from '../interfaces/rate-limit.repository.interface';

export class FirebaseRateLimitRepository implements IRateLimitRepository {
  private basePath = 'rate_limits';

  private getKey(senderId: string, boxId: string): string {
    return `${senderId}_${boxId}`;
  }

  /** Get the rate-limit record for a sender + box pair. */
  async get(senderId: string, boxId: string): Promise<RateLimitRecord | null> {
    const key = this.getKey(senderId, boxId);
    const snapshot = await db.ref(`${this.basePath}/${key}`).once('value');
    if (!snapshot.exists()) return null;
    return snapshot.val() as RateLimitRecord;
  }

  /** Increment count by 1. */
  async increment(senderId: string, boxId: string): Promise<void> {
    const key = this.getKey(senderId, boxId);
    const ref = db.ref(`${this.basePath}/${key}/count`);

    await ref.transaction((current: number | null) => {
      return (current || 0) + 1;
    });
  }

  /** Start a new window: count = 1, window_start = now. */
  async reset(senderId: string, boxId: string): Promise<void> {
    const key = this.getKey(senderId, boxId);
    await db.ref(`${this.basePath}/${key}`).set({
      count: 1,
      window_start: Date.now(),
    });
  }

  /**
   * Atomic check-and-increment (Firebase transaction). Returns { allowed: true }
   * or { allowed: false, remainingMs }.
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
      // No record → create it, allow
      if (!current) {
        return { count: 1, window_start: now };
      }

      // Window expired → reset, allow
      if (now - current.window_start > windowMs) {
        return { count: 1, window_start: now };
      }

      // Limit reached → abort the transaction (return undefined)
      if (current.count >= maxCount) {
        return undefined as any;
      }

      // Under the limit → increment
      return { count: current.count + 1, window_start: current.window_start };
    });

    if (!result.committed) {
      // Aborted transaction = rate limit reached
      const snapshot = await ref.once('value');
      const data = snapshot.val() as RateLimitRecord;
      const remainingMs = data ? windowMs - (now - data.window_start) : 0;
      return { allowed: false, remainingMs: Math.max(0, remainingMs) };
    }

    return { allowed: true };
  }
}
