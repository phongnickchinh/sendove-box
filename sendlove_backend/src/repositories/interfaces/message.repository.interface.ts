import { Message } from '../../types/message.types';

export interface IMessageRepository {
  createMessage(boxId: string, messageId: string, data: Omit<Message, 'id'>): Promise<Message>;
  getMessage(boxId: string, messageId: string): Promise<Message | null>;
  listMessages(boxId: string, limit?: number): Promise<Message[]>;
  countMessagesSince(boxId: string, senderId: string, sinceTimestamp: number): Promise<number>;
  softDelete(boxId: string, messageId: string): Promise<void>;
}
