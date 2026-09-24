/**
 * CRC-32 zlib/IEEE (poly 0xEDB88320) — cùng công thức firmware (SdStore::crc32Update) dùng để
 * kiểm file tải về thẻ (nhạc báo thức, gói theme). Backend tự tính từ file đã lên Storage,
 * không tin con số web gửi.
 */
const CRC_TABLE = (() => {
  const t = new Uint32Array(256);
  for (let n = 0; n < 256; n++) {
    let c = n;
    for (let k = 0; k < 8; k++) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1;
    t[n] = c >>> 0;
  }
  return t;
})();

export function crc32(buf: Buffer): number {
  let c = 0xffffffff;
  for (let i = 0; i < buf.length; i++) c = CRC_TABLE[(c ^ buf[i]) & 0xff] ^ (c >>> 8);
  return (c ^ 0xffffffff) >>> 0;
}
