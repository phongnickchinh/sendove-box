import { SCREEN, BG_BYTES } from '../theme/layout';

/**
 * Ảnh bất kỳ → nền màn chờ của hộp: cắt vuông giữa, thu về 240×240, lượng tử
 * RGB565 (5-6-5 bit) little-endian — đúng định dạng mảng StandbyBackground[]
 * của firmware, hộp không cần bộ giải mã ảnh.
 *
 * previewUrl vẽ lại từ CHÍNH các giá trị 565 đã lượng tử, nên xem trước thấy
 * luôn dải màu bị bậc thang như trên hộp, không đẹp hơn thực tế.
 */
export async function imageToRgb565(file) {
  const bitmap = await createImageBitmap(file);
  const side = Math.min(bitmap.width, bitmap.height);
  const canvas = document.createElement('canvas');
  canvas.width = SCREEN;
  canvas.height = SCREEN;
  const ctx = canvas.getContext('2d');
  ctx.drawImage(bitmap, (bitmap.width - side) / 2, (bitmap.height - side) / 2, side, side, 0, 0, SCREEN, SCREEN);
  bitmap.close?.();

  const img = ctx.getImageData(0, 0, SCREEN, SCREEN);
  const bytes = rgbaToRgb565(img.data);

  // Vẽ ngược lại từ 565 để xem trước đúng những gì hộp sẽ hiện.
  const px = img.data;
  for (let i = 0, j = 0; j < bytes.length; i += 4, j += 2) {
    const v = bytes[j] | (bytes[j + 1] << 8);
    px[i] = ((v >> 11) & 0x1f) * 255 / 31;
    px[i + 1] = ((v >> 5) & 0x3f) * 255 / 63;
    px[i + 2] = (v & 0x1f) * 255 / 31;
  }
  ctx.putImageData(img, 0, 0);

  return { bytes, previewUrl: canvas.toDataURL('image/png') };
}

/** RGBA (Uint8ClampedArray) → RGB565 little-endian, 2 byte mỗi điểm ảnh. Thuần, test được. */
export function rgbaToRgb565(rgba) {
  const out = new Uint8Array((rgba.length / 4) * 2);
  for (let i = 0, j = 0; i < rgba.length; i += 4, j += 2) {
    const v = ((rgba[i] >> 3) << 11) | ((rgba[i + 1] >> 2) << 5) | (rgba[i + 2] >> 3);
    out[j] = v & 0xff;
    out[j + 1] = v >> 8;
  }
  return out;
}

/** RGB565 LE (đúng bytes gửi xuống hộp) → ImageData 240×240 để vẽ xem trước. */
export function rgb565ToImageData(bytes) {
  const img = new ImageData(SCREEN, SCREEN);
  const px = img.data;
  for (let i = 0, j = 0; j < bytes.length; i += 4, j += 2) {
    const v = bytes[j] | (bytes[j + 1] << 8);
    px[i] = ((v >> 11) & 0x1f) * 255 / 31;
    px[i + 1] = ((v >> 5) & 0x3f) * 255 / 63;
    px[i + 2] = (v & 0x1f) * 255 / 31;
    px[i + 3] = 255;
  }
  return img;
}

/**
 * Nền mặc định của hộp (xuất từ StandbyBackground[] cũ của firmware, 2026-09-24): firmware
 * không còn nền biên dịch sẵn, nên "Mặc định" cũng là một ảnh nền web gửi xuống như mọi theme.
 */
export async function loadDefaultBackground(url) {
  const res = await fetch(url);
  const bytes = new Uint8Array(await res.arrayBuffer());
  if (bytes.length !== BG_BYTES) throw new Error(`default bg ${bytes.length} B`);
  const canvas = document.createElement('canvas');
  canvas.width = SCREEN;
  canvas.height = SCREEN;
  canvas.getContext('2d').putImageData(rgb565ToImageData(bytes), 0, 0);
  return { bytes, previewUrl: canvas.toDataURL('image/png'), isDefault: true };
}

export { BG_BYTES };
