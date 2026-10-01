#ifndef ALARM_CLOCK_H
#define ALARM_CLOCK_H

#include <Arduino.h>
#include "ConfigManager.h"
#include "config.h"

// ============================================================================
// AlarmClock — the box's alarm list + the decision of when to ring
// ============================================================================
// Three places touch the list, on three different tasks:
//   - NetworkManager (WakeSync task): downloads from / pushes to the cloud
//   - the captive portal (NetworkController task): add / edit / delete in AP mode
//   - Task_MediaPlayer + the sleep loop (UIController): ask "is it time to ring?"
// so every public method takes the internal mutex. The RAM copy is the read
// source; NVS is written only on change.
//
// Two-way sync rule (product decision):
//   - An edit ON THE BOX (portal, or a one-shot alarm turning itself off after
//     ringing) sets the dirty flag (NVS). The next sync PUSHES THE WHOLE LIST to
//     the cloud, overwriting the cloud copy.
//   - While dirty, the cloud list is NOT accepted (the box wins). Once clean, the
//     cloud is authoritative: a_flag set -> download and replace everything.
//   No per-alarm merge: AP mode has no trustworthy clock to compare updated_at.
// ============================================================================

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
    /// Call after a successful PUT to the cloud with the snapshot taken at `rev`.
    /// If a new edit arrived during the push, dirty stays set for the next sync.
    void markPushed(uint32_t rev);

    /// Call periodically (~500ms). true = start ringing now; outTime receives
    /// "HH:MM" and outItem (if given) a copy of the ringing alarm (music, volume,
    /// ramp). A 5-minute snooze returns the snoozed alarm. A one-shot alarm is
    /// turned off (dirty) the MOMENT it starts ringing.
    bool pollDue(time_t now, char* outTime, size_t len, AlarmItem* outItem = nullptr);

    /// The distinct music_ids in use by alarms, ordered by the soonest alarm first
    /// (download priority). Disabled alarms come last. Returns the number written.
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
