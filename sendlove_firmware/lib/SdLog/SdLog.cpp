#include "SdLog.h"

#include <atomic>

#include "SDCardManager.h"
#include "SdStore.h"

namespace SdLog {

namespace {

// 32 lines × 64 chars = 2KB of RAM. DLOG lines are already cut to the screen width.
constexpr size_t LINES = 32;
constexpr size_t COLS = 64;
constexpr uint32_t ROTATE_BYTES = 64 * 1024;
constexpr const char* LOG0 = "/sys/log/log0.txt";
constexpr const char* LOG1 = "/sys/log/log1.txt";

char s_ring[LINES][COLS];
uint32_t s_written = 0;  // total lines add()ed
uint32_t s_flushed = 0;  // total lines written to the card
bool s_newError = true;  // true at boot: the first push carries [BOOT] reset=
portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;

bool isErrorLine(const char* s) {
    // "reset=" is only worth pushing when it isn't a power-on (1) / software reset (3).
    const char* r = strstr(s, "reset=");
    if (r) return !(r[6] == '1' && r[7] == '\0') && !(r[6] == '3' && r[7] == '\0');
    return strstr(s, "ERR") || strstr(s, "FAIL") || strstr(s, "fail");
}

}  // namespace

void add(const char* line) {
    if (!line) return;
    bool err = isErrorLine(line);
    portENTER_CRITICAL(&s_mux);
    char* slot = s_ring[s_written % LINES];
    strncpy(slot, line, COLS - 1);
    slot[COLS - 1] = '\0';
    s_written++;
    if (err) s_newError = true;
    portEXIT_CRITICAL(&s_mux);
}

void flush() {
    SDCardManager* card = SdStore::card();
    if (!card) return;
    // Two tasks may call this at once (the STANDBY loop and pre-sleep): one doing it is enough.
    static std::atomic<bool> busy{false};
    if (busy.exchange(true)) return;
    struct Release { ~Release() { busy = false; } } release;

    // Snapshot the unwritten lines into a temp buffer before touching the card (keeps
    // the critical section short). The buffer is on the heap: 2KB on the stack would
    // be over half of Task_UIController's 4KB stack.
    char* buf = (char*)malloc(LINES * (COLS + 1));
    if (!buf) return;
    size_t len = 0;
    portENTER_CRITICAL(&s_mux);
    uint32_t from = s_flushed;
    if (s_written - from > LINES) from = s_written - LINES;  // ring overflow: drop the oldest lines
    for (uint32_t i = from; i < s_written; i++) {
        const char* l = s_ring[i % LINES];
        size_t n = strnlen(l, COLS);
        memcpy(buf + len, l, n);
        len += n;
        buf[len++] = '\n';
    }
    uint32_t upTo = s_written;
    portEXIT_CRITICAL(&s_mux);
    if (len == 0) {
        free(buf);
        return;
    }

    int32_t size = card->getFileSize(LOG0);
    if (size > (int32_t)ROTATE_BYTES) {
        card->deleteFile(LOG1);
        card->renameFile(LOG0, LOG1);
    }
    // Do NOT use openGenWrite: that handle belongs to WakeSync, and opening it here
    // would close the music/theme file being downloaded (seen in real logs: "ghi the
    // FAIL" right after "tai").
    if (card->appendFile(LOG0, (const uint8_t*)buf, len) == (int32_t)len) s_flushed = upTo;
    free(buf);
}

bool takeTail(char* out, size_t maxLen) {
    if (!out || maxLen < 2) return false;
    size_t len = 0;
    portENTER_CRITICAL(&s_mux);
    bool want = s_newError;
    if (want) {
        s_newError = false;
        uint32_t from = (s_written > LINES) ? s_written - LINES : 0;
        for (uint32_t i = from; i < s_written && len + COLS + 1 < maxLen; i++) {
            const char* l = s_ring[i % LINES];
            size_t n = strnlen(l, COLS);
            memcpy(out + len, l, n);
            len += n;
            out[len++] = '\n';
        }
    }
    portEXIT_CRITICAL(&s_mux);
    out[len] = '\0';
    return want && len > 0;
}

}  // namespace SdLog
