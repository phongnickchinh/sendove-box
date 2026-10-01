import React, { useEffect, useRef } from 'react';
import Icon from '../ui/Icon';
import { SCREEN, formatDate, isVlw } from '../../theme/layout';
import { parseVlw, drawVlwText } from '../../utils/vlw';
import { rgb565ToImageData } from '../../utils/rgb565';

/**
 * Colors of the PHYSICAL BOX SCREEN — fixed, NOT following the web theme. With
 * var(--caramel-*) the preview background turns dark wine in dark mode while
 * the clock text stays #000000 from the box theme → invisible. The box screen
 * is a physical object with one look. Without a background image the box fills
 * BLACK (the firmware has no built-in background).
 */
const NO_BG = '#000000';
const BOX_INK = '#83513E';

/** A widget's text as on the box: VLW → the subsetted family + size; built-in font → a monospace approximation. */
function textStyle(w) {
  if (isVlw(w)) return { fontFamily: `"${w.family}", var(--font)`, fontSize: w.px, fontWeight: 600 };
  return w.type === 'clock_time'
    ? { fontFamily: 'monospace', fontSize: 40, fontWeight: 700 }
    : { fontFamily: 'monospace', fontSize: 13, fontWeight: 600 };
}

const textOf = (w) => (w.type === 'clock_date' ? formatDate(w) : w.sample);

/**
 * The box's real screen, drawn 1:1 at 240 × 240 so the theme's x/y/w/h read
 * directly off the picture with no conversion.
 *
 * background: the RGB565-quantized background image (utils/rgb565.js).
 */
export default function BoxScreen({ widgets, selectedId, onSelect, onMove, showBoxes, background }) {
  const screenRef = useRef(null);
  const drag = useRef(null); // { id, px, py, x, y, maxX, maxY, k }

  /* Dragging a widget: pointer coordinates are converted to the box's 240 units
     (the preview may render smaller than 240 on narrow phones), rounded to
     integers and clamped to the screen. */
  const startDrag = (e, w) => {
    onSelect?.(w.id);
    if (!onMove) return;
    const rect = screenRef.current.getBoundingClientRect();
    drag.current = {
      id: w.id, px: e.clientX, py: e.clientY, x: w.x, y: w.y,
      maxX: SCREEN - w.w, maxY: SCREEN - w.h, k: SCREEN / rect.width,
    };
    e.currentTarget.setPointerCapture(e.pointerId);
  };
  const moveDrag = (e) => {
    const d = drag.current;
    if (!d) return;
    const clamp = (v, max) => Math.max(0, Math.min(max, Math.round(v)));
    onMove(d.id, {
      x: clamp(d.x + (e.clientX - d.px) * d.k, d.maxX),
      y: clamp(d.y + (e.clientY - d.py) * d.k, d.maxY),
    });
  };
  const endDrag = () => { drag.current = null; };

  return (
    <div
      ref={screenRef}
      style={{
        position: 'relative',
        width: SCREEN, height: SCREEN,
        maxWidth: '100%',
        borderRadius: 8,
        overflow: 'hidden',
        background: background ? `center / cover no-repeat url(${background})` : NO_BG,
        alignSelf: 'center',
        flex: '0 0 auto',
        // Without this, dragging a finger on a phone scrolls the page instead of moving the widget.
        touchAction: onMove ? 'none' : undefined,
      }}
    >
      {widgets.map((w) => {
        const sel = w.id === selectedId;
        const common = {
          position: 'absolute', left: w.x, top: w.y, width: w.w, height: w.h,
          display: 'flex', alignItems: 'center',
          justifyContent: w.align === 'center' ? 'center' : w.align === 'right' ? 'flex-end' : 'flex-start',
        };

        /* The widget outline = the exact w×h box drawBackgroundPatch will clear,
           not decoration. Drawing it is the only way to see a too-small box
           BEFORE the device shows stale text. */
        const box = showBoxes
          ? {
              borderRadius: 4,
              border: sel ? '1.5px solid var(--rose-500)' : `0.5px dashed ${BOX_INK}`,
              background: sel ? 'rgba(253,240,239,.35)' : 'transparent',
            }
          : {};

        let inner;
        if (w.type === 'battery_icon') {
          // The firmware's multi-color battery image — fixed colors, ignores cfg.color.
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
            onPointerDown={(e) => startDrag(e, w)} onPointerMove={moveDrag}
            onPointerUp={endDrag} onPointerCancel={endDrag}
            aria-label={`Chọn ${w.label}`}
            style={{ ...common, ...box, padding: 0, cursor: onMove ? 'grab' : 'pointer', zIndex: sel ? 1 : undefined }}
          >
            {inner}
          </button>
        );
      })}
    </div>
  );
}

/**
 * A preview that matches the box EXACTLY (firmware MEMORY.md §29, proposal #10):
 * drawn from the very bytes to be sent —
 * the RGB565 background and the freshly subsetted VLW fonts, with 0/255 alpha
 * like LovyanGFX. Characters missing from the font show as empty boxes, as on
 * the box. Battery/Wi-Fi widgets only mark their position.
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
 * A REAL thumbnail of a theme: BoxScreen itself (background + text + fonts)
 * scaled from 240 down to `size` with CSS zoom.
 */
export function MiniScreen({ widgets, background, size = 56 }) {
  return (
    <span style={{ flex: '0 0 auto', width: size, height: size, borderRadius: 8, overflow: 'hidden', display: 'block' }}>
      <span style={{ display: 'block', zoom: size / SCREEN, pointerEvents: 'none' }} aria-hidden="true">
        <BoxScreen widgets={widgets} background={background} />
      </span>
    </span>
  );
}
