"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.ThemeController = void 0;
const theme_service_1 = require("../services/theme.service");
class ThemeController {
    constructor(themeService = new theme_service_1.ThemeService()) {
        this.themeService = themeService;
        this.getTheme = async (req, res, next) => {
            try {
                const data = await this.themeService.getTheme(req.params.boxId);
                res.status(200).json({ success: true, data });
            }
            catch (error) {
                next(error);
            }
        };
        this.saveTheme = async (req, res, next) => {
            try {
                const data = await this.themeService.saveTheme(req.params.boxId, req.user.uid, req.body);
                res.status(200).json({ success: true, data });
            }
            catch (error) {
                next(error);
            }
        };
        this.initiateBackground = async (req, res, next) => {
            try {
                const data = await this.themeService.initiateBackgroundUpload(req.params.boxId);
                res.status(200).json({ success: true, data });
            }
            catch (error) {
                next(error);
            }
        };
        this.initiateFont = async (req, res, next) => {
            try {
                const data = await this.themeService.initiateFontUpload(req.params.boxId);
                res.status(200).json({ success: true, data });
            }
            catch (error) {
                next(error);
            }
        };
    }
}
exports.ThemeController = ThemeController;
//# sourceMappingURL=theme.controller.js.map