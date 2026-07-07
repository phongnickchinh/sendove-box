import { Message } from '../../types/message.types';
import { db } from '../../firebase';

export class FirebaseMessageRepository {
  /**
   * Tạo message mới dưới node messages/{boxId}/{messageId}
   */
  async createMessage(boxId: string, messageId: string, data: Omit<Message, 'id'>): Promise<Message> {
    const ref = db.ref(`messages/${boxId}/${messageId}`);
    const record = { id: messageId, ...data };
    await ref.set(record);
    return record as Message;
  }

  /**
   * Lấy 1 message theo ID
   */
  async getMessage(boxId: string, messageId: string): Promise<Message | null> {
    const snapshot = await db.ref(`messages/${boxId}/${messageId}`).once('value');
    if (!snapshot.exists()) return null;
    return snapshot.val() as Message;
  }

  /**
   * Liệt kê N message gần nhất, sắp xếp theo timestamp giảm dần.
   * ESP32 dùng trường timestamp để so sánh với last_download_ts nội bộ.
   */
  async listMessages(boxId: string, limit: number = 20): Promise<Message[]> {
    const snapshot = await db.ref(`messages/${boxId}`)
      .orderByChild('timestamp')
      .limitToLast(limit)
      .once('value');

    if (!snapshot.exists()) return [];

    const messagesObj = snapshot.val();
    const messages: Message[] = [];

    for (const msgId in messagesObj) {
      messages.push(messagesObj[msgId]);
    }

    // Sort descending by timestamp
    return messages.sort((a, b) => b.timestamp - a.timestamp);
  }

  /**
   * Đếm số message trong 1 khoảng thời gian (dùng cho rate limiting)
   */
  async countMessagesSince(boxId: string, senderId: string, sinceTimestamp: number): Promise<number> {
    const snapshot = await db.ref(`messages/${boxId}`)
      .orderByChild('timestamp')
      .startAt(sinceTimestamp)
      .once('value');

    if (!snapshot.exists()) return 0;

    const messagesObj = snapshot.val();
    let count = 0;
    for (const msgId in messagesObj) {
      if (messagesObj[msgId].sender_id === senderId) {
        count++;
      }
    }
    return count;
  }

  /**
   * Xoá mềm message (set deleted_at)
   */
  async softDelete(boxId: string, messageId: string): Promise<void> {
    await db.ref(`messages/${boxId}/${messageId}/deleted_at`).set(Date.now());
  }
}
