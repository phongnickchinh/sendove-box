import { buildVlw, ensureWebFont } from './vlw';
import { FONT_FAMILIES, VLW_KEY, charsetFor, isVlw } from '../theme/layout';

/** Bitmap cap for ONE glyph: the box allocates it on an 8KB task stack. */
export const MAX_GLYPH_BYTES = 3000;

/**
 * Subset the fonts of a theme package: one file per widget type (f_time,
 * f_date), from the FIRST widget's family + size (see sharedFontIssue).
 * Returns { f_time?: {bytes, family, px}, f_date?: ... }.
 */
export async function buildThemeFonts(widgets) {
  const out = {};
  for (const [type, key] of Object.entries(VLW_KEY)) {
    const first = widgets.find((w) => w.type === type && isVlw(w));
    if (!first) continue;
    const fam = FONT_FAMILIES.find((f) => f.family === first.family) || FONT_FAMILIES[0];
    await ensureWebFont(fam.family, fam.weight);
    const res = buildVlw({ family: fam.family, weight: fam.weight, px: first.px, chars: charsetFor(type, widgets) });
    if (res.maxGlyphBytes > MAX_GLYPH_BYTES) {
      throw new Error(`Chữ ${fam.family} cỡ ${first.px}px quá lớn cho hộp — hãy giảm cỡ chữ.`);
    }
    out[key] = { bytes: res.bytes, family: fam.family, px: first.px };
  }
  return out;
}
