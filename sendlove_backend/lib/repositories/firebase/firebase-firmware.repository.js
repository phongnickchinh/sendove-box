"use strict";
var __createBinding = (this && this.__createBinding) || (Object.create ? (function(o, m, k, k2) {
    if (k2 === undefined) k2 = k;
    var desc = Object.getOwnPropertyDescriptor(m, k);
    if (!desc || ("get" in desc ? !m.__esModule : desc.writable || desc.configurable)) {
      desc = { enumerable: true, get: function() { return m[k]; } };
    }
    Object.defineProperty(o, k2, desc);
}) : (function(o, m, k, k2) {
    if (k2 === undefined) k2 = k;
    o[k2] = m[k];
}));
var __setModuleDefault = (this && this.__setModuleDefault) || (Object.create ? (function(o, v) {
    Object.defineProperty(o, "default", { enumerable: true, value: v });
}) : function(o, v) {
    o["default"] = v;
});
var __importStar = (this && this.__importStar) || (function () {
    var ownKeys = function(o) {
        ownKeys = Object.getOwnPropertyNames || function (o) {
            var ar = [];
            for (var k in o) if (Object.prototype.hasOwnProperty.call(o, k)) ar[ar.length] = k;
            return ar;
        };
        return ownKeys(o);
    };
    return function (mod) {
        if (mod && mod.__esModule) return mod;
        var result = {};
        if (mod != null) for (var k = ownKeys(mod), i = 0; i < k.length; i++) if (k[i] !== "default") __createBinding(result, mod, k[i]);
        __setModuleDefault(result, mod);
        return result;
    };
})();
Object.defineProperty(exports, "__esModule", { value: true });
exports.FirebaseFirmwareRepository = void 0;
const firebase_base_repository_1 = require("./firebase-base.repository");
class FirebaseFirmwareRepository extends firebase_base_repository_1.FirebaseBaseRepository {
    constructor() {
        super('firmware');
    }
    /**
     * Lấy firmware mới nhất (version cao nhất)
     */
    async getLatest() {
        const { db } = await Promise.resolve().then(() => __importStar(require('../../firebase')));
        const snapshot = await db.ref(this.collectionPath)
            .orderByChild('created_at')
            .limitToLast(1)
            .once('value');
        if (!snapshot.exists())
            return null;
        const data = snapshot.val();
        const fwId = Object.keys(data)[0];
        return data[fwId];
    }
}
exports.FirebaseFirmwareRepository = FirebaseFirmwareRepository;
//# sourceMappingURL=firebase-firmware.repository.js.map