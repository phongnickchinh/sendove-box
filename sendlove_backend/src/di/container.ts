import { IBoxRepository } from '../repositories/interfaces/box.repository.interface';
import { IUserRepository } from '../repositories/interfaces/user.repository.interface';
import { IMessageRepository } from '../repositories/interfaces/message.repository.interface';
import { IRateLimitRepository } from '../repositories/interfaces/rate-limit.repository.interface';
import { IStorageRepository } from '../repositories/interfaces/storage.repository.interface';
import { IAlarmRepository } from '../repositories/interfaces/alarm.repository.interface';
import { IOtaRepository } from '../repositories/interfaces/ota.repository.interface';
import { IFirmwareRepository } from '../repositories/interfaces/firmware.repository.interface';

import { FirebaseBoxRepository } from '../repositories/firebase/firebase-box.repository';
import { FirebaseUserRepository } from '../repositories/firebase/firebase-user.repository';
import { FirebaseMessageRepository } from '../repositories/firebase/firebase-message.repository';
import { FirebaseRateLimitRepository } from '../repositories/firebase/firebase-rate-limit.repository';
import { FirebaseStorageRepository } from '../repositories/firebase/firebase-storage.repository';
import { FirebaseAlarmRepository } from '../repositories/firebase/firebase-alarm.repository';
import { FirebaseOtaRepository } from '../repositories/firebase/firebase-ota.repository';
import { FirebaseFirmwareRepository } from '../repositories/firebase/firebase-firmware.repository';

import { BoxService } from '../services/box.service';
import { AuthService } from '../services/auth.service';
import { UserService } from '../services/user.service';
import { MessageService } from '../services/message.service';
import { DeviceService } from '../services/device.service';
import { AlarmService } from '../services/alarm.service';
import { MusicService } from '../services/music.service';

import { BoxController } from '../controllers/box.controller';
import { AuthController } from '../controllers/auth.controller';
import { UserController } from '../controllers/user.controller';
import { MessageController } from '../controllers/message.controller';
import { DeviceController } from '../controllers/device.controller';
import { AlarmController } from '../controllers/alarm.controller';
import { MusicController } from '../controllers/music.controller';

export interface AppContainer {
  // Repositories
  boxRepo: IBoxRepository;
  userRepo: IUserRepository;
  messageRepo: IMessageRepository;
  rateLimitRepo: IRateLimitRepository;
  storageRepo: IStorageRepository;
  alarmRepo: IAlarmRepository;
  otaRepo: IOtaRepository;
  fwRepo: IFirmwareRepository;

  // Services
  boxService: BoxService;
  authService: AuthService;
  userService: UserService;
  messageService: MessageService;
  deviceService: DeviceService;
  alarmService: AlarmService;
  musicService: MusicService;

  // Controllers
  boxController: BoxController;
  authController: AuthController;
  userController: UserController;
  messageController: MessageController;
  deviceController: DeviceController;
  alarmController: AlarmController;
  musicController: MusicController;
}

export function createContainer(): AppContainer {
  // 1. Instantiating Repositories
  const boxRepo = new FirebaseBoxRepository();
  const userRepo = new FirebaseUserRepository();
  const messageRepo = new FirebaseMessageRepository();
  const rateLimitRepo = new FirebaseRateLimitRepository();
  const storageRepo = new FirebaseStorageRepository();
  const alarmRepo = new FirebaseAlarmRepository();
  const otaRepo = new FirebaseOtaRepository();
  const fwRepo = new FirebaseFirmwareRepository();

  // 2. Instantiating Services with dependencies injected
  const boxService = new BoxService(boxRepo, userRepo);
  const authService = new AuthService(userRepo, boxRepo);
  const userService = new UserService(userRepo);
  const messageService = new MessageService(messageRepo, storageRepo);
  const deviceService = new DeviceService(boxRepo, messageRepo, alarmRepo, fwRepo, otaRepo);
  const alarmService = new AlarmService(alarmRepo);
  const musicService = new MusicService(); // No deps

  // 3. Instantiating Controllers with dependencies injected
  const boxController = new BoxController(boxService);
  const authController = new AuthController(authService);
  const userController = new UserController(userService);
  const messageController = new MessageController(messageService);
  const deviceController = new DeviceController(deviceService);
  const alarmController = new AlarmController(alarmService);
  const musicController = new MusicController(musicService);

  return {
    boxRepo, userRepo, messageRepo, rateLimitRepo,
    storageRepo, alarmRepo, otaRepo, fwRepo,
    boxService, authService, userService,
    messageService, deviceService, alarmService, musicService,
    boxController, authController, userController,
    messageController, deviceController, alarmController, musicController
  };
}
