#ifndef SD_LOG_H
#define SD_LOG_H

#include <Arduino.h>

// SdLog — a persistent log on the card, with its tail pushed to the cloud on
// errors (MEMORY.md §29, proposal #2).
// add() is called for EVERY DLOG line and only copies into a RAM ring: DLOG may run
// while spiMutex is held, and touching the card there would deadlock (rule R1 in SDCardManager.cpp).
// flush() writes to /sys/log/log0.txt (rotating at 64KB); call it where spiMutex is
// NOT held. takeTail() returns data only after a new error line or the first time
// after boot.

namespace SdLog {

void add(const char* line);
void flush();
/// true + copies the tail ('\n'-separated lines) into out when there is something new worth pushing.
bool takeTail(char* out, size_t maxLen);

}  // namespace SdLog

#endif  // SD_LOG_H
