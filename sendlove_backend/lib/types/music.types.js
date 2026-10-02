"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.MUSIC_LIMITS = void 0;
exports.MUSIC_LIMITS = {
    MAX_TRACKS: 10,
    MIN_MS: 5000,
    MAX_MS: 60000,
    /** Matches the firmware's ALARM_MUSIC_MAX_BYTES: 60s × 16000 × 2 + header < 2MB */
    MAX_BYTES: 2_000_000,
    SAMPLE_RATE: 16000,
    NAME_MAX: 40,
};
//# sourceMappingURL=music.types.js.map