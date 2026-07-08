export interface RateLimitRecord {
  count: number;
  window_start: number;
}

export interface IRateLimitRepository {
  get(senderId: string, boxId: string): Promise<RateLimitRecord | null>;
  increment(senderId: string, boxId: string): Promise<void>;
  reset(senderId: string, boxId: string): Promise<void>;
  checkAndIncrement(
    senderId: string,
    boxId: string,
    maxCount: number,
    windowMs: number
  ): Promise<{ allowed: boolean; remainingMs?: number }>;
}
