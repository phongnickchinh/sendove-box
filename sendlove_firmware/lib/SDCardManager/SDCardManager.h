#ifndef SD_CARD_MANAGER_H
#define SD_CARD_MANAGER_H

#include <Arduino.h>
#include <SD.h>
#include <SPI.h>

// SDCardManager — MicroSD file access under the shared SPI mutex. It only knows
// paths, file handles and the bus; slot / manifest / queue semantics live in
// SDStorageProvider.
//
// Four resident file handles (+ temporaries; SD.begin reserves max_files = 7):
//   _genFile   : general writes (music, theme). Task_WakeSync ONLY: openGenWrite()
//                closes the open handle.
//   _writeFile : message download
//   _readFile  : sequential reads for MediaPlayer
//   _atFile    : random reads for AudioPlayer — MUST be separate, because readAt()
//                must not move the sequential cursor.

class SDCardManager {
public:
    /// Mount the card. spiMutex guards the shared SPI bus. true = mounted.
    bool init(uint8_t csPin, SemaphoreHandle_t spiMutex);

    /// Whether the card is mounted (not being mounted is NOT fatal).
    bool isMounted() const { return _mounted; }

    // --- Write Operations ---

    /// Create or overwrite a file. Returns bytes written, or -1.
    int32_t writeFile(const char* path, const uint8_t* data, size_t len);

    /// Append (create if missing) under one mutex hold. Returns bytes written, or -1.
    int32_t appendFile(const char* path, const uint8_t* data, size_t len);

    /// Open a file for streamed writing (truncates existing content)
    bool openFileForWrite(const char* path);

    /// Reopen a file to write at an offset without truncating (audio after video).
    bool openFileForAppend(const char* path, uint32_t atOffset);

    /// Write a chunk. Returns bytes written; LESS than len signals an error.
    size_t appendChunk(const uint8_t* data, size_t len);

    /// Patch the 4-byte size prefix at offset 0 of the open file.
    bool patchWriteFileAt0(const uint8_t* buf4);

    /// Close the file being written
    void closeWriteFile();

    // --- Sequential Read (MediaPlayer) ---

    /// Open a file for sequential reading
    bool openFileForRead(const char* path);

    /// Move the sequential read cursor
    bool seekReadFile(uint32_t offset);

    /// Read at the sequential cursor. Returns bytes read (0 at EOF / on error).
    size_t readBlock(uint8_t* buffer, size_t len);

    /// Close the sequential read file
    void closeReadFile();

    // --- Random Read (AudioPlayer) — its own handle, NEVER touches the sequential cursor ---

    /// Open the random-read handle on the same path (caches the file size)
    bool openAtFile(const char* path);

    /// Read at an absolute offset, clamped to the physical file size.
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

    /// Read a whole small file. Returns bytes read, or -1 if it couldn't be opened.
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

    /// List a directory (non-recursive). cb gets the bare name + isDir and runs
    /// after the mutex is released, so it may call back into this class.
    size_t listDir(const char* dir, void (*cb)(const char* name, bool isDir, void* ctx), void* ctx);

    /// Free space (MB). 0 if not mounted.
    uint32_t freeMB() const;

    /// Second write handle, for general files (music/theme). append = resume at the end.
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

    /// Create path's parent directory if missing. ASSUMES THE MUTEX IS HELD.
    void mkParentDirLocked(const char* path);
};

#endif // SD_CARD_MANAGER_H
