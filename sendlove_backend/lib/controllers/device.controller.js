"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.DeviceController = void 0;
const device_service_1 = require("../services/device.service");
class DeviceController {
    constructor(deviceService = new device_service_1.DeviceService()) {
        this.deviceService = deviceService;
        this.register = async (req, res, next) => {
            try {
                const data = await this.deviceService.registerDevice(req.body);
                res.status(201).json({ success: true, data });
            }
            catch (error) {
                next(error);
            }
        };
        this.poll = async (req, res, next) => {
            try {
                const boxId = `box_${req.deviceId}`;
                const lastDownloadTs = req.query.last_download_ts
                    ? parseInt(req.query.last_download_ts, 10)
                    : undefined;
                const data = await this.deviceService.poll(boxId, lastDownloadTs);
                res.status(200).json({ success: true, data });
            }
            catch (error) {
                next(error);
            }
        };
        this.heartbeat = async (req, res, next) => {
            try {
                const boxId = `box_${req.deviceId}`;
                await this.deviceService.heartbeat(boxId, req.body);
                res.status(200).json({ success: true, data: { serverTime: new Date().toISOString() } });
            }
            catch (error) {
                next(error);
            }
        };
    }
}
exports.DeviceController = DeviceController;
//# sourceMappingURL=device.controller.js.map