#include "AlarmClock.h"
#include "ScreenLogger.h"
#include <time.h>

// Same as MIN_VALID_EPOCH in NetworkManager.cpp (2020-09-13). Below it the RTC
// was never set -> local time is garbage and alarms must not ring by it.
static constexpr time_t ALARM_MIN_VALID_EPOCH = 1600000000;

AlarmClock& AlarmClock::instance() {
    static AlarmClock s;
    return s;
}

void AlarmClock::lock() {
    if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
}

void AlarmClock::unlock() {
    if (_mutex) xSemaphoreGive(_mutex);
}

void AlarmClock::begin() {
    if (_mutex == nullptr) _mutex = xSemaphoreCreateMutex();

    ConfigManager cfg;
    if (cfg.init(NVS_NAMESPACE)) {
        _count = cfg.loadAlarms(_items, MAX_ALARMS);
        _dirty = cfg.loadAlarmDirty();
        cfg.end();
    }
    // Unpushed edits from the previous run (e.g. edited on the portal, then Wi-Fi
    // saved -> restart): make rev differ from pushedRev so the first sync pushes.
    _rev = _dirty ? 1 : 0;
    _pushedRev = 0;
    DLOG("[ALM] %u alarms, dirty=%d", (unsigned)_count, _dirty ? 1 : 0);
}

bool AlarmClock::isValidTime(const char* t) {
    if (t == nullptr || strlen(t) != 5 || t[2] != ':') return false;
    if (!isdigit((unsigned char)t[0]) || !isdigit((unsigned char)t[1]) ||
        !isdigit((unsigned char)t[3]) || !isdigit((unsigned char)t[4])) return false;
    int h = (t[0] - '0') * 10 + (t[1] - '0');
    int m = (t[3] - '0') * 10 + (t[4] - '0');
    return h < 24 && m < 60;
}

size_t AlarmClock::list(AlarmItem* out, size_t maxCount) {
    lock();
    size_t n = (_count < maxCount) ? _count : maxCount;
    for (size_t i = 0; i < n; i++) out[i] = _items[i];
    unlock();
    return n;
}

void AlarmClock::saveLocked() {
    ConfigManager cfg;
    if (cfg.init(NVS_NAMESPACE)) {
        cfg.saveAlarms(_items, _count);
        cfg.saveAlarmDirty(_dirty);
        cfg.end();
    }
}

void AlarmClock::markDirtyLocked() {
    _rev++;
    _dirty = true;
}

int AlarmClock::findLocked(const char* id) {
    if (id == nullptr || id[0] == '\0') return -1;
    for (size_t i = 0; i < _count; i++) {
        if (strcmp(_items[i].id, id) == 0) return (int)i;
    }
    return -1;
}

bool AlarmClock::replaceFromCloud(const AlarmItem* items, size_t count) {
    lock();
    if (_dirty) {
        unlock();
        DLOG("[ALM] cloud list bo qua: hop con sua doi chua day");
        return false;
    }
    if (count > MAX_ALARMS) count = MAX_ALARMS;
    for (size_t i = 0; i < count; i++) _items[i] = items[i];
    _count = count;
    saveLocked();
    unlock();
    DLOG("[ALM] cloud -> %u alarms", (unsigned)count);
    return true;
}

bool AlarmClock::upsert(const char* id, const char* time, bool enable, bool repeatable,
                        char* outId, size_t outLen) {
    if (!isValidTime(time)) return false;

    lock();
    int idx;
    if (id != nullptr && id[0] != '\0') {
        idx = findLocked(id);
        if (idx < 0) { unlock(); return false; }
    } else {
        if (_count >= MAX_ALARMS) { unlock(); return false; }
        idx = (int)_count;
        AlarmItem fresh;
        // Same id shape the backend generates ("alarm_<ms>") when the clock is valid.
        // In AP mode after a cold boot there is no time yet, so use a random number
        // — it only has to be unique.
        time_t now = ::time(nullptr);
        do {
            if (now >= ALARM_MIN_VALID_EPOCH) {
                snprintf(fresh.id, sizeof(fresh.id), "alarm_%llu",
                         (unsigned long long)now * 1000ULL + (millis() % 1000));
                now++;  // collision (two taps within a second): try the next value
            } else {
                snprintf(fresh.id, sizeof(fresh.id), "alarm_r%08lx", (unsigned long)esp_random());
            }
        } while (findLocked(fresh.id) >= 0);
        _items[idx] = fresh;
        _count++;
    }

    strncpy(_items[idx].time, time, sizeof(_items[idx].time) - 1);
    _items[idx].time[sizeof(_items[idx].time) - 1] = '\0';
    _items[idx].isEnable = enable;
    _items[idx].repeatable = repeatable;
    if (outId && outLen) {
        strncpy(outId, _items[idx].id, outLen - 1);
        outId[outLen - 1] = '\0';
    }
    markDirtyLocked();
    saveLocked();
    unlock();
    return true;
}

bool AlarmClock::remove(const char* id) {
    lock();
    int idx = findLocked(id);
    if (idx < 0) { unlock(); return false; }
    for (size_t i = (size_t)idx; i + 1 < _count; i++) _items[i] = _items[i + 1];
    _count--;
    markDirtyLocked();
    saveLocked();
    unlock();
    return true;
}

bool AlarmClock::isDirty(uint32_t* rev) {
    lock();
    bool d = _dirty;
    if (rev) *rev = _rev;
    unlock();
    return d;
}

void AlarmClock::markPushed(uint32_t rev) {
    lock();
    _pushedRev = rev;
    if (_rev == rev && _dirty) {
        _dirty = false;
        saveLocked();
    }
    unlock();
}

bool AlarmClock::pollDue(time_t now, char* outTime, size_t len, AlarmItem* outItem) {
    if (now < ALARM_MIN_VALID_EPOCH) return false;

    lock();
    if (_snoozeUntil != 0 && now >= _snoozeUntil) {
        _snoozeUntil = 0;
        strncpy(outTime, _snoozeTime, len - 1);
        outTime[len - 1] = '\0';
        if (outItem) *outItem = _snoozeItem;
        unlock();
        DLOG("[ALM] snooze het -> keu lai %s", outTime);
        return true;
    }

    time_t minuteKey = now / 60;
    if (minuteKey == _lastFiredMinute) { unlock(); return false; }

    struct tm tmNow;
    localtime_r(&now, &tmNow);
    char hhmm[6];
    snprintf(hhmm, sizeof(hhmm), "%02d:%02d", tmNow.tm_hour, tmNow.tm_min);

    for (size_t i = 0; i < _count; i++) {
        if (!_items[i].isEnable || strcmp(_items[i].time, hhmm) != 0) continue;

        _lastFiredMinute = minuteKey;
        _snoozeUntil = 0;  // a new alarm overrides a pending snooze
        strncpy(_snoozeTime, hhmm, sizeof(_snoozeTime));  // label in case the user snoozes
        _snoozeItem = _items[i];                          // a snooze rings again with this same music
        if (outItem) *outItem = _items[i];
        // Two alarms in the same minute: the first in the list wins. Not a mandatory case (MEMORY.md §28).
        bool repeatable = _items[i].repeatable;
        if (!repeatable) {
            // Turned off the moment it starts ringing (not on dismiss): even after a
            // power loss midway it won't ring again tomorrow. Snooze still works
            // because it doesn't read isEnable.
            _items[i].isEnable = false;
            markDirtyLocked();
            saveLocked();
        }
        strncpy(outTime, hhmm, len - 1);
        outTime[len - 1] = '\0';
        unlock();
        DLOG("[ALM] RING %s (%s)", hhmm, repeatable ? "moi ngay" : "mot lan");
        return true;
    }
    unlock();
    return false;
}

void AlarmClock::snooze(time_t now) {
    lock();
    // pollDue() already set the _snoozeTime label when ringing started.
    _snoozeUntil = now + ALARM_SNOOZE_SEC;
    unlock();
    DLOG("[ALM] snooze %us", (unsigned)ALARM_SNOOZE_SEC);
}

void AlarmClock::dismiss() {
    lock();
    _snoozeUntil = 0;
    _snoozeTime[0] = '\0';
    unlock();
    DLOG("[ALM] tat");
}

size_t AlarmClock::musicInUse(time_t now, char (*outIds)[24], size_t maxCount) {
    struct Cand {
        const char* id;
        uint32_t key;  // seconds until it rings; disabled alarm / no clock yet = very large
    };
    Cand c[MAX_ALARMS];
    size_t n = 0;

    lock();
    struct tm tmNow;
    bool clockOk = now >= ALARM_MIN_VALID_EPOCH;
    int32_t curSec = 0;
    if (clockOk) {
        localtime_r(&now, &tmNow);
        curSec = tmNow.tm_hour * 3600 + tmNow.tm_min * 60 + tmNow.tm_sec;
    }
    for (size_t i = 0; i < _count; i++) {
        if (_items[i].musicId[0] == '\0') continue;
        uint32_t key = 0xFFFFFFF0u;
        if (_items[i].isEnable && clockOk && isValidTime(_items[i].time)) {
            const char* t = _items[i].time;
            int32_t diff = ((t[0] - '0') * 10 + (t[1] - '0')) * 3600 +
                           ((t[3] - '0') * 10 + (t[4] - '0')) * 60 - curSec;
            if (diff < 0) diff += 86400;
            key = (uint32_t)diff;
        }
        c[n++] = {_items[i].musicId, key};
    }
    // Insertion sort (≤ 10 items).
    for (size_t i = 1; i < n; i++) {
        Cand v = c[i];
        size_t j = i;
        while (j > 0 && c[j - 1].key > v.key) {
            c[j] = c[j - 1];
            j--;
        }
        c[j] = v;
    }
    size_t out = 0;
    for (size_t i = 0; i < n && out < maxCount; i++) {
        bool dup = false;
        for (size_t k = 0; k < out; k++) {
            if (strcmp(outIds[k], c[i].id) == 0) dup = true;
        }
        if (dup) continue;
        strncpy(outIds[out], c[i].id, 23);
        outIds[out][23] = '\0';
        out++;
    }
    unlock();
    return out;
}

uint32_t AlarmClock::secondsToNext(time_t now) {
    if (now < ALARM_MIN_VALID_EPOCH) return 0xFFFFFFFF;

    lock();
    uint32_t best = 0xFFFFFFFF;

    if (_snoozeUntil != 0) {
        best = (_snoozeUntil > now) ? (uint32_t)(_snoozeUntil - now) : 0;
    }

    struct tm tmNow;
    localtime_r(&now, &tmNow);
    int32_t curSec = tmNow.tm_hour * 3600 + tmNow.tm_min * 60 + tmNow.tm_sec;
    bool firedThisMinute = (now / 60) == _lastFiredMinute;

    for (size_t i = 0; i < _count; i++) {
        if (!_items[i].isEnable || !isValidTime(_items[i].time)) continue;
        const char* t = _items[i].time;
        int32_t alarmSec = ((t[0] - '0') * 10 + (t[1] - '0')) * 3600
                         + ((t[3] - '0') * 10 + (t[4] - '0')) * 60;
        int32_t diff = alarmSec - curSec;

        // Already in the alarm minute but pollDue() hasn't run yet (e.g. woke a few
        // ms early). Return 0 so the sleep loop does NOT sleep again -> no missed alarm.
        if (diff <= 0 && diff > -60 && !firedThisMinute) {
            best = 0;
            break;
        }
        if (diff <= 0) {
            // Already passed today. A one-shot alarm that is still on (it couldn't
            // ring because the box was powered off) also waits for tomorrow —
            // pollDue rings it, then it turns itself off.
            diff += 86400;
        }
        if ((uint32_t)diff < best) best = (uint32_t)diff;
    }
    unlock();
    return best;
}
