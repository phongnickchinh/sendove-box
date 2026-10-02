#ifndef ALARM_CLOCK_H
#define ALARM_CLOCK_H

#include <Arduino.h>
#include "ConfigManager.h"
#include "config.h"

// AlarmClock — the alarm list + the decision of when to ring. Used from three
// tasks (sync, portal, player/sleep loop), so every public method takes the
// internal mutex. RAM is the read source; NVS is written only on change.
//
// Two-way sync (product decision): an edit ON THE BOX sets the dirty flag and the
// next sync PUSHES THE WHOLE LIST (box wins, cloud list not accepted while dirty).
// Once clean, the cloud is authoritative. No per-alarm merge: AP mode has no
// trustworthy clock.

class AlarmClock {
public:
    static AlarmClock& instance();

    /// Create the mutex + load the list from NVS. Call once in setup(), before creating tasks.
    void begin();

    /// Copy the current list out. Returns the item count.
    size_t list(AlarmItem* out, size_t maxCount);

    /// Cloud -> box. Returns false and writes NOTHING if the box has unpushed edits.
    bool replaceFromCloud(const AlarmItem* items, size_t count);

    /// Portal: an empty id = add (the generated id goes to outId). Returns false on
    /// a malformed time, an unknown id, or when MAX_ALARMS is reached.
    bool upsert(const char* id, const char* time, bool enable, bool repeatable,
                char* outId = nullptr, size_t outLen = 0);
    bool remove(const char* id);

    /// Whether there are edits not yet pushed to the cloud. `rev` is for markPushed().
    bool isDirty(uint32_t* rev);
    /// After a successful PUT of the snapshot taken at `rev`; stays dirty if an
    /// edit arrived meanwhile.
    void markPushed(uint32_t rev);

    /// Poll (~500ms). true = start ringing now; outTime gets "HH:MM", outItem a copy
    /// of the alarm. A one-shot alarm is turned off (dirty) the moment it rings.
    bool pollDue(time_t now, char* outTime, size_t len, AlarmItem* outItem = nullptr);

    /// Distinct music_ids in use, soonest alarm first (download priority). Returns the count.
    size_t musicInUse(time_t now, char (*outIds)[24], size_t maxCount);

    /// Short touch while ringing: ring again after ALARM_SNOOZE_SEC.
    void snooze(time_t now);
    /// Hold / ALARM_RING_MAX_MS elapsed: stop for good and cancel a pending snooze.
    void dismiss();

    /// Seconds to the next ring (snooze included). 0 = due but not ringing yet.
    /// 0xFFFFFFFF = nothing to ring, or the clock isn't valid yet.
    uint32_t secondsToNext(time_t now);

    /// A real 24h "HH:MM". Shared by the portal and the cloud path.
    static bool isValidTime(const char* t);

private:
    AlarmClock() = default;

    SemaphoreHandle_t _mutex = nullptr;
    AlarmItem _items[MAX_ALARMS];
    size_t    _count = 0;

    uint32_t _rev = 0;        // bumped on every edit made on the box
    uint32_t _pushedRev = 0;  // the rev already pushed
    bool     _dirty = false;

    time_t   _lastFiredMinute = 0;  // prevents ringing twice within one minute
    time_t   _snoozeUntil = 0;
    char     _snoozeTime[6] = "";
    AlarmItem _snoozeItem;          // the snoozed alarm: rings again with the same music + volume

    void lock();
    void unlock();
    void saveLocked();          // writes the list + the dirty flag to NVS
    void markDirtyLocked();
    int  findLocked(const char* id);
};

#endif // ALARM_CLOCK_H
