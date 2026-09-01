import React from 'react';
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

/** chrome(frame, stepLabel) — appbar. step = null thì bỏ pill bước. */
export function AppBar({ onBack, step, right }) {
  return (
    <div className="sl-appbar">
      {onBack && (
        <button type="button" className="sl-iconbtn" onClick={onBack} aria-label="Quay lại">
          <Icon name="back" size={24} />
        </button>
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

/** hdr(title, to) */
export function Header({ title, to }) {
  return (
    <div className="sl-hdr">
      <h1 className="sl-title">{title}</h1>
      {to && (
        <div className="sl-hdr__to">
          <Icon name="heart" size={16} />
          <span className="sl-caption">{to}</span>
        </div>
      )}
    </div>
  );
}

/** tips(text) */
export function Tips({ children }) {
  return (
    <div className="sl-tips">
      <Icon name="shield" size={16} />
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

/** scrim(frame) + modal(frame) — lớp phủ và popup, chỗ duy nhất có đổ bóng */
export function Modal({ children }) {
  return (
    <div className="sl-scrim">
      <div className="sl-modal">{children}</div>
    </div>
  );
}
