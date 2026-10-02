import React, { useEffect } from 'react';
import Icon from './Icon';
import '../../styles/tokens.css';
import '../../styles/sendlove.css';

/**
 * Screen building blocks, mapped 1:1 from the Figma script helpers
 * (chrome/body/acts/hdr/tips/btn/chip/circle/modal). The 430x932 frame is a
 * 100dvh flex column here.
 */

/** Device frame: page background + the 430 column, centered on wide screens */
export function Screen({ children }) {
  return (
    <div className="sl-page">
      <div className="sl-shell">{children}</div>
    </div>
  );
}

/**
 * The appbar. step = null hides the step pill. title/subtitle = the Dashboard
 * variant (greeting inside the appbar). Use either onBack or title, not both.
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

/** body(frame, gap) — screen body, padding [8,20,10,20] */
export function Body({ center = false, children }) {
  return (
    <div className={`sl-body-col${center ? ' sl-body-col--center' : ''}`}>{children}</div>
  );
}

/** acts(frame, buttons) — button row pinned to the bottom */
export function Actions({ children }) {
  return <div className="sl-acts">{children}</div>;
}

/** hdr(title, to) — 12px icon, matching solar:mailbox-linear in Figma (not 16px). */
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

/** tips(text) — 12px icon, matching material-symbols:privacy-tip-outline-rounded. */
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

/** chip(iconKey) — 44x44 tile on rose/200 */
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
 * Overlay + popup. onClose (optional): clicking outside or Esc closes it; omit
 * it while the popup runs something that must not be interrupted.
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
