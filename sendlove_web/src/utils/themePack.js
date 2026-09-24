import { buildVlw, ensureWebFont } from './vlw';
import { FONT_FAMILIES, VLW_KEY, charsetFor, isVlw } from '../theme/layout';

/**
 * Trần bitmap MỘT glyph: VLWfont::drawChar của hộp cấp nó bằng alloca trên stack task vẽ
 * (8KB, config.h TASK_STACK_MEDIA_PLAYER). Vượt thì chặn trước khi gửi.
 */
export const MAX_GLYPH_BYTES = 3000;

/**
 * Cắt phông cho gói theme: một file cho mọi widget giờ (f_time), một cho mọi widget ngày
 * (f_date), theo họ + cỡ của widget ĐẦU TIÊN mỗi loại (theme/layout.js sharedFontIssue).
 * Trả { f_time?: {bytes, family, px}, f_date?: ... } — chỉ loại nào có widget dùng VLW.
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
