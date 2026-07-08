"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.MessageController = void 0;
const message_service_1 = require("../services/message.service");
class MessageController {
    constructor(msgService = new message_service_1.MessageService()) {
        this.msgService = msgService;
        this.initiateMessage = async (req, res, next) => {
            try {
                const { boxId } = req.params;
                const uid = req.user.uid;
                const { types } = req.body;
                const data = await this.msgService.initiateMessage(boxId, uid, types);
                res.status(201).json({ success: true, data });
            }
            catch (error) {
                next(error);
            }
        };
        this.confirmMessage = async (req, res, next) => {
            try {
                const { boxId } = req.params;
                const uid = req.user.uid;
                const data = await this.msgService.confirmMessage(boxId, uid, req.body);
                res.status(200).json({
                    success: true,
                    data
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
    }
}
exports.MessageController = MessageController;
//# sourceMappingURL=message.controller.js.map