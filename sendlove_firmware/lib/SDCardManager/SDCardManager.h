#ifndef SD_CARD_MANAGER_H
#define SD_CARD_MANAGER_H

#include <Arduino.h>
#include <SD.h>
#include <SPI.h>

// ============================================================================
// SDCardManager — MicroSD file system access + the SPI mutex
// ============================================================================
// Shared module, used by:
// - the network layer (writes files downloaded from Firebase)
// - MediaPlayer (reads files for playback)
//
// Every SPI operation is wrapped in xSemaphoreTake/Give(spiMutex) to avoid
// clashing with DisplayDriver (same SPI bus).
//
// MODULE BOUNDARY: this file only knows paths, file handles and the SPI bus.
// All slot / manifest / queue semantics live in SDStorageProvider — the same
// split as NandStorage / NandStorageProvider.
//
// FOUR resident FILE HANDLES + the temporary one of readFile/readFileAt
// (SD.begin reserves max_files = 7):
//   _genFile   : general file writes (music, theme) for WakeSync, separate from the
//                message download path. Task_WakeSync ONLY: openGenWrite() closes
//                the open handle, so another task sharing it would hijack the file
//                being downloaded (the log uses appendFile() instead).
//   _writeFile : the write (download) path
//   _readFile  : sequential reads for MediaPlayer (cursor managed by the provider)
//   _atFile    : random reads for AudioPlayer — MUST be a SEPARATE handle, because
//                readAt() must not move the sequential cursor (see readAt in
//                IStorageProvider.h). Seek-then-seek-back on a shared handle is
//                exactly the bug that rule exists to prevent.
// ============================================================================

class SDCardManager {
public:
    /// Initialize the SD card
    /// @param csPin Chip Select pin of the SD module
    /// @param spiMutex mutex guarding the shared SPI bus (created in main.cpp)
    /// @return true if the card mounted
    bool init(uint8_t csPin, SemaphoreHandle_t spiMutex);

    /// Whether the card is mounted. Failing to mount is NOT fatal: the card is
    /// removable, and the provider must keep running in empty mode.
    bool isMounted() const { return _mounted; }

    // --- Write Operations ---

    /// Write data to a file (create or overwrite)
    /// @return bytes written, or -1 on error
    int32_t writeFile(const char* path, const uint8_t* data, size_t len);

    /// Append to a file (create if missing); open-write-close under one mutex hold
    /// @return bytes written, or -1 on error
    int32_t appendFile(const char* path, const uint8_t* data, size_t len);

    /// Open a file for streamed writing (truncates existing content)
    bool openFileForWrite(const char* path);

    /// Reopen an existing file to keep writing at a given offset (mode "r+", no truncation).
    /// Used to append audio after the video.
    bool openFileForAppend(const char* path, uint32_t atOffset);

    /// Write another chunk to the open file
    /// @return bytes written. Returns LESS than len on error — the signal
    ///         NetworkManager uses to detect a writeError.
    size_t appendChunk(const uint8_t* data, size_t len);

    /// Patch 4 bytes at offset 0 of the open file (the payload size prefix).
    /// Mode "w" is O_TRUNC — truncation happens at OPEN, not at write, so
    /// overwriting 4 bytes at the start can't shorten the file.
    bool patchWriteFileAt0(const uint8_t* buf4);

    /// Close the file being written
    void closeWriteFile();

    // --- Sequential Read (MediaPlayer) ---

    /// Open a file for sequential reading
    bool openFileForRead(const char* path);

    /// Move the sequential read cursor
    bool seekReadFile(uint32_t offset);

    /// Read a block at the sequential cursor
    /// @return bytes actually read (0 at end of file / on error)
    size_t readBlock(uint8_t* buffer, size_t len);

    /// Close the sequential read file
    void closeReadFile();

    // --- Random Read (AudioPlayer) — its own handle, NEVER touches the sequential cursor ---

    /// Open the random-read handle on the same path (caches the file size)
    bool openAtFile(const char* path);

    /// Read at an absolute offset, clamped to the physical file size.
    /// Skips the seek when the offset equals the current position — AudioPlayer
    /// reads monotonically in AUDIO_READ_CHUNK_SIZE steps, and seeking every time
    /// would defeat readahead.
    int readAtFile(uint32_t offset, uint8_t* buffer, uint32_t len);

    /// Close the random-read handle
    void closeAtFile();

    /// Size of the file open on the random-read handle (0 if none)
    uint32_t atFileSize() const { return _atSize; }

    // --- Utility ---

    /// Whether a file exists
    bool fileExists(const char* path) const;

    /// Delete a file. Returns false if it doesn't exist or the delete failed.
    bool deleteFile(const char* path);

    /// File size in bytes, or -1 if it doesn't exist
    int32_t getFileSize(const char* path) const;

    /// Read a whole small file (manifest / caption) into a buffer
    /// @return bytes read, or -1 if it couldn't be opened
    int32_t readFile(const char* path, uint8_t* buf, size_t maxLen) const;

    // --- General files (theme, alarm music, log) ---

    /// Unmount and remount the card (it was just reinserted). Call only with NO handle open.
    bool remount();

    /// Whether the card still responds (opens a file and reads one byte). If not -> marked unmounted.
    bool probe();

    /// Read `len` bytes at `offset` of a file (open-read-close). -1 if it couldn't be opened.
    int32_t readFileAt(const char* path, uint32_t offset, uint8_t* buf, size_t len) const;

    /// Rename. FAT doesn't overwrite: returns false if `to` exists (the caller deletes it first).
    bool renameFile(const char* from, const char* to);

    /// Create a directory (and one level of parent). true if it already exists or was created.
    bool makeDir(const char* path);

    /// Remove an EMPTY directory (SD.rmdir). For a whole tree: SdStore::removeTree().
    bool removeDir(const char* path);

    /// List a directory's entries (non-recursive). cb receives the name WITHOUT the path
    /// and an isDir flag. Names are copied out first and cb runs after the mutex is
    /// released, so cb may call back into this class (delete, rename).
    size_t listDir(const char* dir, void (*cb)(const char* name, bool isDir, void* ctx), void* ctx);

    /// Free space (MB). 0 if not mounted.
    uint32_t freeMB() const;

    /// A SECOND write handle for general files (music/theme downloads) — separate from
    /// the message download path's _writeFile. append = open "a" (continue at the end,
    /// for resuming with Range).
    bool openGenWrite(const char* path, bool append);
    size_t genWrite(const uint8_t* data, size_t len);
    void closeGenWrite();

private:
    uint8_t _csPin = 0;
    mutable SemaphoreHandle_t _spiMutex = nullptr;
    bool _mounted = false;

    File _writeFile;
    File _readFile;
    File _atFile;
    File _genFile;  // general file writes (see openGenWrite)

    uint32_t _atSize = 0;
    uint32_t _atPos = 0;
    bool _atPosKnown = false;

    /// Acquire the SPI bus (blocking, 1s timeout) + the NOP hack for the ST7789
    bool acquireSPI() const;

    /// Release the SPI bus
    void releaseSPI() const;

    /// Create path's parent directory if missing. ASSUMES THE MUTEX IS HELD
    /// (it is non-recursive — never acquire it nested).
    void mkParentDirLocked(const char* path);
};

#endif // SD_CARD_MANAGER_H
