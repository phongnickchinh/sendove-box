import { db } from '../../firebase';

interface RateLimitRecord {
  count: number;
  window_start: number;
}

export class FirebaseRateLimitRepository {
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
}
