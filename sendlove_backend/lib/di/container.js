"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.createContainer = createContainer;
const firebase_box_repository_1 = require("../repositories/firebase/firebase-box.repository");
const firebase_user_repository_1 = require("../repositories/firebase/firebase-user.repository");
const firebase_message_repository_1 = require("../repositories/firebase/firebase-message.repository");
const firebase_rate_limit_repository_1 = require("../repositories/firebase/firebase-rate-limit.repository");
const firebase_storage_repository_1 = require("../repositories/firebase/firebase-storage.repository");
const firebase_alarm_repository_1 = require("../repositories/firebase/firebase-alarm.repository");
const firebase_ota_repository_1 = require("../repositories/firebase/firebase-ota.repository");
const firebase_firmware_repository_1 = require("../repositories/firebase/firebase-firmware.repository");
const box_service_1 = require("../services/box.service");
const auth_service_1 = require("../services/auth.service");
const user_service_1 = require("../services/user.service");
const message_service_1 = require("../services/message.service");
const device_service_1 = require("../services/device.service");
const alarm_service_1 = require("../services/alarm.service");
const music_service_1 = require("../services/music.service");
const box_controller_1 = require("../controllers/box.controller");
const auth_controller_1 = require("../controllers/auth.controller");
const user_controller_1 = require("../controllers/user.controller");
const message_controller_1 = require("../controllers/message.controller");
const device_controller_1 = require("../controllers/device.controller");
const alarm_controller_1 = require("../controllers/alarm.controller");
const music_controller_1 = require("../controllers/music.controller");
function createContainer() {
    // 1. Instantiating Repositories
    const boxRepo = new firebase_box_repository_1.FirebaseBoxRepository();
    const userRepo = new firebase_user_repository_1.FirebaseUserRepository();
    const messageRepo = new firebase_message_repository_1.FirebaseMessageRepository();
    const rateLimitRepo = new firebase_rate_limit_repository_1.FirebaseRateLimitRepository();
    const storageRepo = new firebase_storage_repository_1.FirebaseStorageRepository();
    const alarmRepo = new firebase_alarm_repository_1.FirebaseAlarmRepository();
    const otaRepo = new firebase_ota_repository_1.FirebaseOtaRepository();
    const fwRepo = new firebase_firmware_repository_1.FirebaseFirmwareRepository();
    // 2. Instantiating Services with dependencies injected
    const boxService = new box_service_1.BoxService(boxRepo, userRepo);
    const authService = new auth_service_1.AuthService(userRepo, boxRepo);
    const userService = new user_service_1.UserService(userRepo);
    const messageService = new message_service_1.MessageService(messageRepo, storageRepo);
    const deviceService = new device_service_1.DeviceService(boxRepo, messageRepo, alarmRepo, fwRepo, otaRepo);
    const alarmService = new alarm_service_1.AlarmService(alarmRepo);
    const musicService = new music_service_1.MusicService(); // No deps
    // 3. Instantiating Controllers with dependencies injected
    const boxController = new box_controller_1.BoxController(boxService);
    const authController = new auth_controller_1.AuthController(authService);
    const userController = new user_controller_1.UserController(userService);
    const messageController = new message_controller_1.MessageController(messageService);
    const deviceController = new device_controller_1.DeviceController(deviceService);
    const alarmController = new alarm_controller_1.AlarmController(alarmService);
    const musicController = new music_controller_1.MusicController(musicService);
    return {
        boxRepo, userRepo, messageRepo, rateLimitRepo,
        storageRepo, alarmRepo, otaRepo, fwRepo,
        boxService, authService, userService,
        messageService, deviceService, alarmService, musicService,
        boxController, authController, userController,
        messageController, deviceController, alarmController, musicController
    };
}
//# sourceMappingURL=container.js.map