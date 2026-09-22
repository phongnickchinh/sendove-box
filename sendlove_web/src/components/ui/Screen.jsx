import React, { useEffect } from 'react';
import Icon from './Icon';
import '../../styles/tokens.css';
import '../../styles/sendlove.css';

/**
 * Các khối dựng màn hình, dịch 1-1 từ helper của figma-scripts/_prelude.js.
 * chrome()/body()/acts()/hdr()/tips()/btn()/chip()/circle()/modal().
 *
 * Khác biệt duy nhất so với Figma: thiết kế đặt mọi thứ theo toạ độ tuyệt đối
 * trong khung 430x932. Ở đây khung đó là một cột flex cao 100dvh, nên hàng nút
 * dính đáy bằng margin-top:auto chứ không phải y = 932 - 34 - h.
 */

/** Khung máy: nền trang + cột 430, canh giữa khi màn hình rộng */
export function Screen({ children }) {
  return (
    <div className="sl-page">
      <div className="sl-shell">{children}</div>
    </div>
  );
}

/**
 * chrome(frame, stepLabel) — appbar. step = null thì bỏ pill bước.
 * title/subtitle: biến thể Dashboard — "Hi, {name}!" + phụ đề nằm NGAY TRONG
 * appbar (đúng frame maindashboard-share-page), không phải trong Body.
 * onBack và title loại trừ nhau ở hai đầu trái của thanh — chỉ dùng một.
 */
export function AppBar({ onBack, title, subtitle, step, right }) {
  return (
    <div className="sl-appbar">
      {onBack && (
        <button type="button" className="sl-back" onClick={onBack} aria-label="Quay lại">
          <Icon name="back" size={22} sw={2} />
        </button>
      )}
      {title && (
        <div className="sl-appbar__title">
          <span className="sl-title" style={{ fontSize: 22 }}>{title}</span>
          {subtitle && <span className="sl-caption" style={{ color: 'var(--neutral-400)' }}>{subtitle}</span>}
        </div>
      )}
      <span className="sl-appbar__spacer" />
      {step && <span className="sl-step">{step}</span>}
      {right}
    </div>
  );
}

/** body(frame, gap) — thân màn hình, pad [8,20,10,20] */
export function Body({ center = false, children }) {
  return (
    <div className={`sl-body-col${center ? ' sl-body-col--center' : ''}`}>{children}</div>
  );
}

/** acts(frame, buttons) — hàng nút ghim đáy */
export function Actions({ children }) {
  return <div className="sl-acts">{children}</div>;
}

/** hdr(title, to) — icon 12px, khớp solar:mailbox-linear trong Figma (không phải 16px). */
export function Header({ title, to }) {
  return (
    <div className="sl-hdr">
      <h1 className="sl-title">{title}</h1>
      {to && (
        <div className="sl-hdr__to">
          <Icon name="mailbox" size={12} />
          <span className="sl-caption">{to}</span>
        </div>
      )}
    </div>
  );
}

/** tips(text) — icon 12px, khớp material-symbols:privacy-tip-outline-rounded. */
export function Tips({ children }) {
  return (
    <div className="sl-tips">
      <Icon name="shield" size={12} />
      <span>{children}</span>
    </div>
  );
}

/** btn(label, kind) — kind: 'pri' | 'gho' */
export function Button({ kind = 'gho', block = true, children, ...rest }) {
  return (
    <button
      type="button"
      className={`sl-btn sl-btn--${kind}${block ? ' sl-btn--block' : ''}`}
      {...rest}
    >
      {children}
    </button>
  );
}

/** chip(iconKey) — ô 44x44 nền rose/200 */
export function Chip({ icon }) {
  return (
    <span className="sl-chip">
      <Icon name={icon} size={24} />
    </span>
  );
}

/** circle(size, bg, icon) */
export function CircleIcon({ size, bg, color, icon, iconSize, sw }) {
  return (
    <span
      className="sl-circle"
      style={{ width: size, height: size, background: bg, color }}
    >
      <Icon name={icon} size={iconSize} sw={sw} />
    </span>
  );
}

/**
 * scrim(frame) + modal(frame) — lớp phủ và popup, chỗ duy nhất có đổ bóng.
 * onClose (tuỳ chọn): bấm ra ngoài hoặc Esc thì đóng. Bỏ trống khi popup đang
 * chạy việc không được ngắt (đang huỷ ghép, đang mã hoá).
 */
export function Modal({ children, onClose, className = '' }) {
  useEffect(() => {
    if (!onClose) return undefined;
    const onKey = (e) => { if (e.key === 'Escape') onClose(); };
    window.addEventListener('keydown', onKey);
    return () => window.removeEventListener('keydown', onKey);
  }, [onClose]);

  return (
    <div className="sl-scrim" onClick={onClose ? (e) => { if (e.target === e.currentTarget) onClose(); } : undefined}>
      <div className={`sl-modal ${className}`} role="dialog" aria-modal="true">{children}</div>
    </div>
  );
}
