import { buildVlw, ensureWebFont } from './vlw';
import { FONT_FAMILIES, VLW_KEY, charsetFor, isVlw } from '../theme/layout';

/**
 * Bitmap cap for ONE glyph: the box's VLWfont::drawChar allocates it with
 * alloca on the draw-task stack (8KB, config.h TASK_STACK_MEDIA_PLAYER).
 * Anything larger is rejected before sending.
 */
export const MAX_GLYPH_BYTES = 3000;

/**
 * Subset the fonts for a theme package: one file for all time widgets (f_time),
 * one for all date widgets (f_date), using the family + size of the FIRST
 * widget of each type (see sharedFontIssue in theme/layout.js).
 * Returns { f_time?: {bytes, family, px}, f_date?: ... } — only types with a VLW widget.
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
