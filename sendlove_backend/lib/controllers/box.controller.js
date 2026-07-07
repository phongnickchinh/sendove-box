"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.BoxController = void 0;
const box_service_1 = require("../services/box.service");
class BoxController {
    constructor() {
        this.pairBox = async (req, res, next) => {
            try {
                const uid = req.user.uid;
                const { pairingCode, boxName } = req.body;
                const data = await this.boxService.pairBox(uid, pairingCode, boxName || 'My Box');
                res.status(200).json({ success: true, data });
            }
            catch (error) {
                next(error);
            }
        };
        this.unpairBox = async (req, res, next) => {
            try {
                const uid = req.user.uid;
                const { boxId } = req.params;
                const role = await this.boxService.unpairBox(uid, boxId);
                res.status(200).json({ success: true, data: { boxId, unpairedRole: role } });
            }
            catch (error) {
                next(error);
            }
        };
        this.getBoxDetails = async (req, res, next) => {
            try {
                const uid = req.user.uid;
                const { boxId } = req.params;
                const data = await this.boxService.getBoxDetails(uid, boxId);
                res.status(200).json({ success: true, data });
            }
            catch (error) {
                next(error);
            }
        };
        this.updateWifi = async (req, res, next) => {
            try {
                const uid = req.user.uid;
                const { boxId } = req.params;
                const { ssid, password } = req.body;
                await this.boxService.updateWifi(uid, boxId, ssid, password);
                res.status(200).json({
                    success: true,
                    data: { status: 'pending', message: 'WiFi config queued. Box will apply on next poll.' }
                });
            }
            catch (error) {
                next(error);
            }
        };
        this.boxService = new box_service_1.BoxService();
    }
}
exports.BoxController = BoxController;
//# sourceMappingURL=box.controller.js.map