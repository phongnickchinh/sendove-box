/**
 * Subsets a font into a VLW file for the box (SD-card design, firmware MEMORY.md §28).
 *
 * The format matches LovyanGFX VLWfont::loadFont / drawChar (lgfx_fonts.cpp) —
 * every integer is 32-bit BIG-endian:
 *   24 B header   : glyphCount, version(11), size(px), 0, ascent, descent
 *   28 B per glyph: unicode, height, width, xAdvance, dY (glyph top above the
 *                   baseline), dX (left edge relative to the cursor), 0
 *   then the glyph bitmaps back to back, 1 alpha byte per pixel (w*h)
 * Glyphs MUST be sorted by unicode: the box looks them up with lower_bound.
 * width/xAdvance are uint8 and dX is int8 on the box → font size is capped by
 * PX_RANGE (theme/layout.js).
 *
 * Alpha is BINARY (0 or 255), no anti-aliasing: the box's ST7789 can't read
 * pixels back, so LovyanGFX blends partial alpha against ONE fixed background
 * color instead of the background image → glyph edges would smear black. With
 * 0/255 a pixel is either drawn or skipped.
 */

const GOOGLE_CSS = 'https://fonts.googleapis.com/css2';

/** Load a Google font family (with its Vietnamese subset) once and wait until canvas can draw it. */
export async function ensureWebFont(family, weight) {
  const id = `gf-${family.replace(/\s+/g, '-')}-${weight}`;
  if (!document.getElementById(id)) {
    const link = document.createElement('link');
    link.id = id;
    link.rel = 'stylesheet';
    link.href = `${GOOGLE_CSS}?family=${encodeURIComponent(family)}:wght@${weight}&display=swap`;
    document.head.appendChild(link);
  }
  // Load exactly the unicode blocks in use (Vietnamese lives in its own subset).
  await document.fonts.load(`${weight} 32px "${family}"`, 'Thứ ảắ 0123456789');
}

const be32 = (view, off, v) => view.setInt32(off, v | 0, false);

/**
 * @param {{family:string, weight:number, px:number, chars:string}} opts
 * @returns {{bytes: Uint8Array, maxGlyphBytes: number, missing: string[]}}
 */
export function buildVlw({ family, weight, px, chars }) {
  const codes = [...new Set([...chars.normalize('NFC')].map((c) => c.codePointAt(0)))]
    .filter((c) => c > 0x20 && c <= 0xffff)
    .sort((a, b) => a - b);

  const canvas = document.createElement('canvas');
  const size = px * 3;
  canvas.width = size;
  canvas.height = size;
  const ctx = canvas.getContext('2d', { willReadFrequently: true });
  ctx.font = `${weight} ${px}px "${family}"`;
  ctx.textBaseline = 'alphabetic';
  ctx.fillStyle = '#fff';

  const ref = ctx.measureText('dp');
  const fontAscent = Math.ceil(ctx.measureText('d').actualBoundingBoxAscent || px * 0.75);
  const fontDescent = Math.ceil(ref.actualBoundingBoxDescent || px * 0.25);

  const glyphs = [];
  const missing = [];
  let maxGlyphBytes = 0;
  const ox = px; // draw origin inset from the edge so glyphs overflowing left/up stay on the canvas
  const oy = px * 2;

  for (const code of codes) {
    const ch = String.fromCodePoint(code);
    const m = ctx.measureText(ch);
    const left = Math.ceil(m.actualBoundingBoxLeft);
    const right = Math.ceil(m.actualBoundingBoxRight);
    const asc = Math.ceil(m.actualBoundingBoxAscent);
    const desc = Math.ceil(m.actualBoundingBoxDescent);
    const w = Math.max(0, left + right);
    const h = Math.max(0, asc + desc);
    const adv = Math.round(m.width);
    if (w === 0 || h === 0 || w > 255 || adv > 255) {
      if (w > 255 || adv > 255) missing.push(ch);
      glyphs.push({ code, w: 0, h: 0, adv: Math.min(adv, 255), dY: 0, dX: 0, bmp: new Uint8Array(0) });
      continue;
    }
    ctx.clearRect(0, 0, size, size);
    ctx.fillText(ch, ox, oy);
    const img = ctx.getImageData(ox - left, oy - asc, w, h).data;
    const bmp = new Uint8Array(w * h);
    for (let i = 0; i < w * h; i++) bmp[i] = img[i * 4 + 3] >= 128 ? 255 : 0;
    maxGlyphBytes = Math.max(maxGlyphBytes, w * h);
    glyphs.push({ code, w, h, adv, dY: asc, dX: -left, bmp });
  }

  const bmpBytes = glyphs.reduce((s, g) => s + g.bmp.length, 0);
  const out = new Uint8Array(24 + glyphs.length * 28 + bmpBytes);
  const view = new DataView(out.buffer);
  be32(view, 0, glyphs.length);
  be32(view, 4, 11);
  be32(view, 8, px);
  be32(view, 12, 0);
  be32(view, 16, fontAscent);
  be32(view, 20, fontDescent);
  let off = 24;
  for (const g of glyphs) {
    be32(view, off, g.code);
    be32(view, off + 4, g.h);
    be32(view, off + 8, g.w);
    be32(view, off + 12, g.adv);
    be32(view, off + 16, g.dY);
    be32(view, off + 20, g.dX);
    be32(view, off + 24, 0);
    off += 28;
  }
  for (const g of glyphs) {
    out.set(g.bmp, off);
    off += g.bmp.length;
  }
  return { bytes: out, maxGlyphBytes, missing };
}

/** Parse a VLW file (the very bytes sent to the box) to draw the preview. */
export function parseVlw(bytes) {
  const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  const n = view.getInt32(0, false);
  const glyphs = new Map();
  let bmpOff = 24 + n * 28;
  let maxAscent = view.getInt32(16, false);
  let maxDescent = view.getInt32(20, false);
  const px = view.getInt32(8, false);
  // loadFont derives spaceWidth from the header, BEFORE updating the max from each glyph.
  const spaceWidth = Math.floor(Math.max(px, maxAscent + maxDescent) * 2 / 7);
  for (let i = 0; i < n; i++) {
    const o = 24 + i * 28;
    const g = {
      h: view.getInt32(o + 4, false), w: view.getInt32(o + 8, false),
      adv: view.getInt32(o + 12, false), dY: view.getInt32(o + 16, false), dX: view.getInt32(o + 20, false),
      off: bmpOff,
    };
    glyphs.set(view.getInt32(o, false), g);
    bmpOff += g.w * g.h;
    maxAscent = Math.max(maxAscent, g.dY);
    maxDescent = Math.max(maxDescent, g.h - g.dY);
  }
  return { bytes, glyphs, maxAscent, maxDescent, spaceWidth };
}

/**
 * Draw `text` the way LovyanGFX does on the box: middle_* datum alignment,
 * alpha-255 pixels drawn as-is, a missing glyph drawn as an empty box (the box
 * calls drawCharDummy and doesn't crash — mandatory case, firmware MEMORY.md §28).
 */
export function drawVlwText(ctx, font, text, x, y, color, align) {
  const codes = [...text.normalize('NFC')].map((c) => c.codePointAt(0));
  const height = font.maxAscent + font.maxDescent;
  let width = 0;
  for (const c of codes) width += c === 0x20 ? font.spaceWidth : (font.glyphs.get(c)?.adv ?? font.spaceWidth);
  let cx = align === 'center' ? x - width / 2 : align === 'right' ? x - width : x;
  const top = Math.round(y - height / 2);
  ctx.fillStyle = color;
  for (const c of codes) {
    const g = font.glyphs.get(c);
    if (c === 0x20 || !g) {
      if (!g && c !== 0x20) ctx.strokeRect(cx + 1, top + 1, font.spaceWidth - 2, height - 2);
      cx += font.spaceWidth;
      continue;
    }
    const gx = Math.round(cx + g.dX);
    const gy = top + (font.maxAscent - g.dY);
    for (let r = 0; r < g.h; r++) {
      for (let q = 0; q < g.w; q++) {
        if (font.bytes[g.off + r * g.w + q] === 255) ctx.fillRect(gx + q, gy + r, 1, 1);
      }
    }
    cx += g.adv;
  }
}
