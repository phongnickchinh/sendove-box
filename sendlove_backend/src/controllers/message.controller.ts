import { NextFunction, Response } from 'express';
import { AuthenticatedRequest, ApiResponse } from '../types/api.types';
import { MessageService } from '../services/message.service';

export class MessageController {
  private msgService: MessageService;

  constructor() {
    this.msgService = new MessageService();
  }

  public createMessage = async (req: AuthenticatedRequest, res: Response<ApiResponse>, next: NextFunction) => {
    try {
      const { boxId } = req.params;
      const data = await this.msgService.createMessage(boxId, req.body);
      res.status(201).json({ success: true, data });
    } catch (error) {
      next(error);
    }
  };

  public completeUpload = async (req: AuthenticatedRequest, res: Response<ApiResponse>, next: NextFunction) => {
    try {
      const { boxId, msgId } = req.params;
      const { uploadedFields } = req.body;
      await this.msgService.completeUpload(boxId, msgId, uploadedFields || []);
      res.status(202).json({
        success: true,
        data: { messageId: msgId, status: 'processing' }
      });
    } catch (error) {
      next(error);
    }
  };

  public getMessages = async (req: AuthenticatedRequest, res: Response<ApiResponse>, next: NextFunction) => {
    try {
      const { boxId } = req.params;
      const limit = req.query.limit ? parseInt(req.query.limit as string) : 20;
      const messages = await this.msgService.getMessages(boxId, limit);
      res.status(200).json({ success: true, data: { messages, pagination: { limit, total: messages.length } } });
    } catch (error) {
      next(error);
    }
  };

  public getMessageDetails = async (req: AuthenticatedRequest, res: Response<ApiResponse>, next: NextFunction) => {
    try {
      const { boxId, msgId } = req.params;
      const message = await this.msgService.getMessageDetails(boxId, msgId);
      res.status(200).json({ success: true, data: message });
    } catch (error) {
      next(error);
    }
  };
}
