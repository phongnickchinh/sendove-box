"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.AlarmController = void 0;
const alarm_service_1 = require("../services/alarm.service");
class AlarmController {
    constructor(alarmService = new alarm_service_1.AlarmService()) {
        this.alarmService = alarmService;
        this.createAlarm = async (req, res, next) => {
            try {
                const { boxId } = req.params;
                const data = await this.alarmService.createAlarm(boxId, req.body);
                res.status(201).json({ success: true, data });
            }
            catch (error) {
                next(error);
            }
        };
        this.getAlarms = async (req, res, next) => {
            try {
                const { boxId } = req.params;
                const data = await this.alarmService.getAlarms(boxId);
                res.status(200).json({ success: true, data });
            }
            catch (error) {
                next(error);
            }
        };
        this.updateAlarm = async (req, res, next) => {
            try {
                const { boxId, alarmId } = req.params;
                const data = await this.alarmService.updateAlarm(boxId, alarmId, req.body);
                res.status(200).json({ success: true, data });
            }
            catch (error) {
                next(error);
            }
        };
        this.deleteAlarm = async (req, res, next) => {
            try {
                const { boxId, alarmId } = req.params;
                await this.alarmService.deleteAlarm(boxId, alarmId);
                res.status(200).json({ success: true, data: null });
            }
            catch (error) {
                next(error);
            }
        };
    }
}
exports.AlarmController = AlarmController;
//# sourceMappingURL=alarm.controller.js.map