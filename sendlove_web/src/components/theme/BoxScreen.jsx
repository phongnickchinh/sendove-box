import React from 'react';
import Icon from '../ui/Icon';
import { SCREEN } from '../../theme/layout';

/**
 * Màu của MÀN HỘP THẬT — cố định, KHÔNG theo theme web. Dùng var(--caramel-*)
 * thì ở dark mode nền xem trước thành nâu/rượu vang tối, còn chữ giờ vẫn
 * #000000 theo theme của hộp → biến mất. Màn hộp là vật thật, một màu duy nhất.
 */
const BOX_BG = '#F6DFB3';
const BOX_INK = '#83513E';

/**
 * Màn hình thật của hộp, vẽ 1:1 ở 240 × 240 để x/y/w/h trong theme đọc thẳng
 * được trên hình, không phải quy đổi.
 *
 * background: ảnh nền đã lượng tử RGB565 (utils/rgb565.js). Không có thì tô
 * màu thay thế — nền mặc định thật là mảng StandbyBackground[] biên dịch trong
 * firmware, web không có bản sao của nó.
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
        background: background ? `center / cover no-repeat url(${background})` : BOX_BG,
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
          const big = w.type === 'clock_time';
          inner = (
            <span style={{
              fontFamily: w.font === 'Orbitron_32' ? "'Orbitron', var(--font)" : 'var(--font)',
              fontSize: big ? (w.font === 'Orbitron_32' ? 32 : 40) : 12,
              fontWeight: big ? 700 : 600,
              lineHeight: big ? 1.1 : 1.3,
              color: w.color,
              whiteSpace: 'nowrap',
            }}>{w.sample}</span>
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
 * Thu nhỏ 240 xuống size — chỉ để nhận mặt, không đọc chữ nên mỗi widget là
 * một khối đặc đặt đúng toạ độ đã quy đổi.
 */
export function Thumb({ widgets, size = 40 }) {
  const k = size / SCREEN;
  return (
    <span style={{
      flex: '0 0 auto', position: 'relative',
      width: size, height: size,
      borderRadius: 8, background: BOX_BG, overflow: 'hidden',
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
