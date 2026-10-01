#ifndef SD_LOG_H
#define SD_LOG_H

#include <Arduino.h>

// ============================================================================
// SdLog — a persistent log on the card + pushing its tail to the cloud on errors
// (MEMORY.md §29, proposal #2)
// ============================================================================
// ScreenLogger::log() calls add() for EVERY DLOG line. add() only copies into a RAM
// ring (it never touches the card): DLOG is called from every task, sometimes while
// holding spiMutex, and writing the card there would deadlock (rule R1 in
// SDCardManager.cpp).
//
// flush() writes the unwritten lines to /sys/log/log0.txt and rotates to log1.txt
// past 64KB. Call it from places that do NOT hold spiMutex: Task_MediaPlayer's
// STANDBY loop and right before sleep.
//
// takeTail(): the tail of the log, to push to status/log_tail. It returns data only
// when there is a new error line since the last take, or the first time after boot
// (so the `[BOOT] reset=` line of the last reboot is visible).
// ============================================================================

namespace SdLog {

void add(const char* line);
void flush();
/// true + copies the tail ('\n'-separated lines) into out when there is something new worth pushing.
bool takeTail(char* out, size_t maxLen);

}  // namespace SdLog

#endif  // SD_LOG_H
