import { Message } from '../../types/message.types';
import { db } from '../../firebase';

export class FirebaseMessageRepository {
  // Messages are stored under `messages/{boxId}/{messageId}`
  
  async createMessage(boxId: string, messageId: string, data: Partial<Message>): Promise<Message> {
    const ref = db.ref(`messages/${boxId}/${messageId}`);
    await ref.set(data);
    return this.getMessage(boxId, messageId) as Promise<Message>;
  }

  async getMessage(boxId: string, messageId: string): Promise<Message | null> {
    const snapshot = await db.ref(`messages/${boxId}/${messageId}`).once('value');
    if (!snapshot.exists()) return null;
    return { messageId, ...snapshot.val() } as Message;
  }

  async updateMessage(boxId: string, messageId: string, data: Partial<Message>): Promise<Message> {
    const ref = db.ref(`messages/${boxId}/${messageId}`);
    await ref.update(data);
    return this.getMessage(boxId, messageId) as Promise<Message>;
  }
  
  async listMessages(boxId: string, limit: number = 20): Promise<Message[]> {
    const snapshot = await db.ref(`messages/${boxId}`)
      .orderByChild('createdAt')
      .limitToLast(limit)
      .once('value');
      
    if (!snapshot.exists()) return [];
    
    const messagesObj = snapshot.val();
    const messages: Message[] = [];
    
    for (const msgId in messagesObj) {
      messages.push({ messageId: msgId, ...messagesObj[msgId] });
    }
    
    // Sort descending by createdAt
    return messages.sort((a, b) => b.createdAt - a.createdAt);
  }
}
