import { SCREEN, BG_BYTES } from '../theme/layout';

/**
 * Any image → the box's standby background: center-crop to a square, scale to
 * 240×240, quantize to little-endian RGB565 (5-6-5 bits) — the format the
 * firmware draws directly, so the box needs no image decoder.
 *
 * previewUrl is redrawn from the quantized 565 values THEMSELVES, so the
 * preview shows the same color banding as the box, no prettier than reality.
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

  // Draw back from 565 so the preview shows exactly what the box will.
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

/** RGBA (Uint8ClampedArray) → little-endian RGB565, 2 bytes per pixel. Pure, testable. */
export function rgbaToRgb565(rgba) {
  const out = new Uint8Array((rgba.length / 4) * 2);
  for (let i = 0, j = 0; i < rgba.length; i += 4, j += 2) {
    const v = ((rgba[i] >> 3) << 11) | ((rgba[i + 1] >> 2) << 5) | (rgba[i + 2] >> 3);
    out[j] = v & 0xff;
    out[j + 1] = v >> 8;
  }
  return out;
}

/** RGB565 LE (the exact bytes sent to the box) → 240×240 ImageData for the preview. */
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
 * The box's default background (exported from the firmware's old
 * StandbyBackground[]): the firmware no longer has a compiled-in background, so
 * "Default" is also an image the web sends down like any other theme.
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
