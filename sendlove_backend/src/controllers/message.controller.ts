import { NextFunction, Response } from 'express';
import { AuthenticatedRequest, ApiResponse } from '../types/api.types';
import { MessageService } from '../services/message.service';

export class MessageController {
  constructor(
    private msgService: MessageService = new MessageService()
  ) {}

  public initiateMessage = async (req: AuthenticatedRequest, res: Response<ApiResponse>, next: NextFunction) => {
    try {
      const { boxId } = req.params;
      const uid = req.user!.uid;
      const { types } = req.body;
      const data = await this.msgService.initiateMessage(boxId, uid, types);
      res.status(201).json({ success: true, data });
    } catch (error) {
      next(error);
    }
  };

  public confirmMessage = async (req: AuthenticatedRequest, res: Response<ApiResponse>, next: NextFunction) => {
    try {
      const { boxId } = req.params;
      const uid = req.user!.uid;
      const data = await this.msgService.confirmMessage(boxId, uid, req.body);
      res.status(200).json({
        success: true,
        data
      });
    } catch (error) {
      next(error);
    }
  };

  public getMessages = async (req: AuthenticatedRequest, res: Response<ApiResponse>, next: NextFunction) => {
    try {
      const { boxId } = req.params;
      // Kẹp 1..100: limit=abc ra NaN làm limitToLast() ném lỗi 500, còn limit
      // quá lớn thì kéo cả cây messages/{boxId} về một lượt.
      const parsed = parseInt(req.query.limit as string, 10);
      const limit = Number.isFinite(parsed) ? Math.min(Math.max(parsed, 1), 100) : 20;
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
