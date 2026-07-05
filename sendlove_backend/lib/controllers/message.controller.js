"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.MessageController = void 0;
const message_service_1 = require("../services/message.service");
class MessageController {
    constructor() {
        this.createMessage = async (req, res, next) => {
            try {
                const { boxId } = req.params;
                const data = await this.msgService.createMessage(boxId, req.body);
                res.status(201).json({ success: true, data });
            }
            catch (error) {
                next(error);
            }
        };
        this.completeUpload = async (req, res, next) => {
            try {
                const { boxId, msgId } = req.params;
                const { uploadedFields } = req.body;
                await this.msgService.completeUpload(boxId, msgId, uploadedFields || []);
                res.status(202).json({
                    success: true,
                    data: { messageId: msgId, status: 'processing' }
                });
            }
            catch (error) {
                next(error);
            }
        };
        this.getMessages = async (req, res, next) => {
            try {
                const { boxId } = req.params;
                const limit = req.query.limit ? parseInt(req.query.limit) : 20;
                const messages = await this.msgService.getMessages(boxId, limit);
                res.status(200).json({ success: true, data: { messages, pagination: { limit, total: messages.length } } });
            }
            catch (error) {
                next(error);
            }
        };
        this.getMessageDetails = async (req, res, next) => {
            try {
                const { boxId, msgId } = req.params;
                const message = await this.msgService.getMessageDetails(boxId, msgId);
                res.status(200).json({ success: true, data: message });
            }
            catch (error) {
                next(error);
            }
        };
        this.msgService = new message_service_1.MessageService();
    }
}
exports.MessageController = MessageController;
//# sourceMappingURL=message.controller.js.map