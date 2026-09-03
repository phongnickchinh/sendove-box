import React from 'react';
import Icon from '../ui/Icon';
import { SCREEN } from '../../theme/layout';

/**
 * Màn hình thật của hộp, vẽ 1:1 ở 240 × 240 để x/y/w/h trong theme đọc thẳng
 * được trên hình, không phải quy đổi.
 *
 * Nền ở đây là màu thay thế: nền thật là mảng StandbyBackground[] biên dịch
 * trong firmware (LayoutEngine.cpp pushImage), KHÔNG phải file đọc từ thẻ.
 */
export default function BoxScreen({ widgets, selectedId, onSelect, showBoxes }) {
  return (
    <div
      style={{
        position: 'relative',
        width: SCREEN, height: SCREEN,
        maxWidth: '100%',
        borderRadius: 8,
        overflow: 'hidden',
        background: 'var(--caramel-100)',
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
              border: sel ? '1.5px solid var(--rose-500)' : '0.5px dashed var(--caramel-700)',
              background: sel ? 'rgba(253,240,239,.35)' : 'transparent',
            }
          : {};

        const inner = w.type === 'battery_icon'
          ? <Icon name="battery" size={16} style={{ color: '#3D2A20' }} />
          : (
            <span style={{
              fontFamily: 'var(--font)',
              fontSize: w.type === 'clock_time' ? 40 : 12,
              fontWeight: w.type === 'clock_time' ? 700 : 600,
              lineHeight: w.type === 'clock_time' ? 1.1 : 1.3,
              color: w.color,
              whiteSpace: 'nowrap',
            }}>{w.sample}</span>
          );

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

/** Thu nhỏ 240 xuống size — chỉ để nhận mặt, không đọc chữ nên vẽ bằng khối đặc. */
export function Thumb({ bars, size = 40 }) {
  const pad = Math.round(size / 8);
  return (
    <span style={{
      flex: '0 0 auto',
      width: size, height: size, padding: pad,
      display: 'flex', flexDirection: 'column', gap: Math.round(pad / 2),
      borderRadius: 8, background: 'var(--caramel-100)',
    }}>
      {bars.map(([w, h], i) => (
        <span key={i} style={{
          width: Math.round((w / 40) * size), height: Math.round((h / 40) * size),
          background: 'var(--caramel-700)', borderRadius: 1,
        }} />
      ))}
    </span>
  );
}
