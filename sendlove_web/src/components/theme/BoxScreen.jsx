import React, { useEffect, useRef } from 'react';
import Icon from '../ui/Icon';
import { SCREEN, formatDate, isVlw } from '../../theme/layout';
import { parseVlw, drawVlwText } from '../../utils/vlw';
import { rgb565ToImageData } from '../../utils/rgb565';

/**
 * Màu của MÀN HỘP THẬT — cố định, KHÔNG theo theme web. Dùng var(--caramel-*)
 * thì ở dark mode nền xem trước thành nâu/rượu vang tối, còn chữ giờ vẫn
 * #000000 theo theme của hộp → biến mất. Màn hộp là vật thật, một màu duy nhất.
 * Không có ảnh nền thì hộp tô ĐEN (firmware không còn nền dựng sẵn từ 2026-09-24).
 */
const NO_BG = '#000000';
const BOX_INK = '#83513E';

/** Chữ của widget như trên hộp: VLW -> họ + cỡ đã cắt; phông có sẵn -> xấp xỉ monospace. */
function textStyle(w) {
  if (isVlw(w)) return { fontFamily: `"${w.family}", var(--font)`, fontSize: w.px, fontWeight: 600 };
  return w.type === 'clock_time'
    ? { fontFamily: 'monospace', fontSize: 40, fontWeight: 700 }
    : { fontFamily: 'monospace', fontSize: 13, fontWeight: 600 };
}

const textOf = (w) => (w.type === 'clock_date' ? formatDate(w) : w.sample);

/**
 * Màn hình thật của hộp, vẽ 1:1 ở 240 × 240 để x/y/w/h trong theme đọc thẳng
 * được trên hình, không phải quy đổi.
 *
 * background: ảnh nền đã lượng tử RGB565 (utils/rgb565.js).
 */
export default function BoxScreen({ widgets, selectedId, onSelect, showBoxes, background }) {
  return (
    <div
      style={{
        position: 'relative',
        width: SCREEN, height: SCREEN,
        maxWidth: '100%',
        borderRadius: 8,
        overflow: 'hidden',
        background: background ? `center / cover no-repeat url(${background})` : NO_BG,
        alignSelf: 'center',
        flex: '0 0 auto',
      }}
    >
      {widgets.map((w) => {
        const sel = w.id === selectedId;
        const common = {
          position: 'absolute', left: w.x, top: w.y, width: w.w, height: w.h,
          display: 'flex', alignItems: 'center',
          justifyContent: w.align === 'center' ? 'center' : w.align === 'right' ? 'flex-end' : 'flex-start',
        };

        /* Khung widget = đúng ô w×h mà drawBackgroundPatch sẽ xoá, không phải
           viền trang trí. Vẽ nó ra là cách duy nhất để thấy ô quá nhỏ TRƯỚC khi
           hộp bị dính chữ cũ. */
        const box = showBoxes
          ? {
              borderRadius: 4,
              border: sel ? '1.5px solid var(--rose-500)' : `0.5px dashed ${BOX_INK}`,
              background: sel ? 'rgba(253,240,239,.35)' : 'transparent',
            }
          : {};

        let inner;
        if (w.type === 'battery_icon') {
          // Ảnh pin nhiều màu của firmware — màu cố định, không theo cfg.color.
          inner = <Icon name="battery" size={16} style={{ color: '#3D2A20' }} />;
        } else if (w.type === 'wifi_icon') {
          inner = <Icon name="wifi" size={18} style={{ color: w.color }} />;
        } else {
          inner = (
            <span style={{ ...textStyle(w), lineHeight: 1.1, color: w.color, whiteSpace: 'nowrap' }}>
              {textOf(w)}
            </span>
          );
        }

        if (!onSelect) return <div key={w.id} style={{ ...common, ...box }}>{inner}</div>;
        return (
          <button
            key={w.id} type="button" onClick={() => onSelect(w.id)}
            aria-label={`Chọn ${w.label}`}
            style={{ ...common, ...box, padding: 0, cursor: 'pointer' }}
          >
            {inner}
          </button>
        );
      })}
    </div>
  );
}

/**
 * Xem trước ĐÚNG như hộp (đề xuất #10): vẽ bằng chính bytes sẽ gửi — nền RGB565 và file
 * phông VLW vừa cắt, alpha 0/255 như LovyanGFX. Ký tự thiếu trong phông hiện ô trống, y
 * như hộp. Widget pin/Wi-Fi chỉ đánh dấu vị trí.
 */
export function ExactPreview({ widgets, fonts, bgBytes }) {
  const ref = useRef(null);
  useEffect(() => {
    const ctx = ref.current?.getContext('2d');
    if (!ctx) return;
    if (bgBytes) ctx.putImageData(rgb565ToImageData(bgBytes), 0, 0);
    else {
      ctx.fillStyle = NO_BG;
      ctx.fillRect(0, 0, SCREEN, SCREEN);
    }
    const parsed = {
      clock_time: fonts?.f_time ? parseVlw(fonts.f_time.bytes) : null,
      clock_date: fonts?.f_date ? parseVlw(fonts.f_date.bytes) : null,
    };
    for (const w of widgets) {
      const cy = w.y + w.h / 2;
      const ax = w.align === 'center' ? w.x + w.w / 2 : w.align === 'right' ? w.x + w.w : w.x;
      const font = isVlw(w) ? parsed[w.type] : null;
      if (font) {
        ctx.strokeStyle = w.color;
        drawVlwText(ctx, font, textOf(w), ax, cy, w.color, w.align);
      } else if (w.type === 'clock_time' || w.type === 'clock_date' || w.type === 'chip_temp') {
        const s = textStyle(w);
        ctx.font = `${s.fontWeight} ${s.fontSize}px ${s.fontFamily}`;
        ctx.fillStyle = w.color;
        ctx.textAlign = w.align === 'center' ? 'center' : w.align === 'right' ? 'right' : 'left';
        ctx.textBaseline = 'middle';
        ctx.fillText(textOf(w), ax, cy);
      } else {
        ctx.strokeStyle = BOX_INK;
        ctx.strokeRect(w.x + 0.5, w.y + 0.5, w.w - 1, w.h - 1);
      }
    }
  }, [widgets, fonts, bgBytes]);
  return (
    <canvas ref={ref} width={SCREEN} height={SCREEN}
      style={{ width: SCREEN, height: SCREEN, maxWidth: '100%', borderRadius: 8, alignSelf: 'center', imageRendering: 'pixelated' }} />
  );
}

/**
 * Thu nhỏ 240 xuống size — chỉ để nhận mặt, không đọc chữ nên mỗi widget là
 * một khối đặc đặt đúng toạ độ đã quy đổi.
 */
export function Thumb({ widgets, size = 40 }) {
  const k = size / SCREEN;
  return (
    <span style={{
      flex: '0 0 auto', position: 'relative',
      width: size, height: size,
      borderRadius: 8, background: '#F6DFB3', overflow: 'hidden',
    }}>
      {widgets.map((w, i) => (
        <span key={i} style={{
          position: 'absolute',
          left: w.x * k, top: w.y * k,
          width: Math.max(2, w.w * k), height: Math.max(2, w.h * k * 0.7),
          background: BOX_INK, borderRadius: 1,
        }} />
      ))}
    </span>
  );
}
