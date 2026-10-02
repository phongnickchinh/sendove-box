import React from 'react';

/**
 * Thin-line illustrations for empty states and send results. Colors come from
 * tokens, set through style (SVG attributes don't accept var(--…)).
 */
const INK = { stroke: 'var(--caramel-800)', strokeWidth: 2, strokeLinecap: 'round', strokeLinejoin: 'round' };
const BODY = { ...INK, fill: 'var(--caramel-100)' };
const ACCENT = { fill: 'var(--rose-400)' };
const SOFT = { fill: 'var(--chip-accent-bg)' };
const LINE = { ...INK, fill: 'none' };

const heart = (x, y, s, style = ACCENT) => (
  <path style={style} transform={`translate(${x} ${y}) scale(${s})`}
    d="M0 3C0-1 5-2 6 1.5 7-2 12-1 12 3 12 7 6 10.5 6 10.5S0 7 0 3z" />
);

const SCENES = {
  // A waiting gift box with hearts floating up: no messages yet.
  inbox: (
    <>
      <ellipse cx="80" cy="108" rx="44" ry="5" style={SOFT} />
      <rect x="44" y="58" width="72" height="46" rx="6" style={BODY} />
      <rect x="38" y="46" width="84" height="16" rx="5" style={BODY} />
      <rect x="74" y="46" width="12" height="58" style={{ ...ACCENT, opacity: 0.85 }} />
      <path style={LINE} d="M80 46c-6-10-20-10-18-2 1 5 12 4 18 2zM80 46c6-10 20-10 18-2-1 5-12 4-18 2z" />
      {heart(94, 18, 1.3)}
      {heart(58, 24, 0.9, { ...ACCENT, opacity: 0.6 })}
      <path style={{ ...LINE, strokeDasharray: '2 5' }} d="M100 40c4-6 2-10-1-12" />
    </>
  ),
  // A sleeping bell: no alarms set.
  alarm: (
    <>
      <ellipse cx="80" cy="108" rx="40" ry="5" style={SOFT} />
      <path style={BODY} d="M52 92c6-6 6-14 6-26 0-14 10-24 22-24s22 10 22 24c0 12 0 20 6 26z" />
      <circle cx="80" cy="98" r="6" style={{ ...ACCENT, ...INK, fill: 'var(--rose-400)' }} />
      <path style={LINE} d="M80 42v-6" />
      <path style={LINE} d="M40 52c-4 6-4 14 0 20M120 52c4 6 4 14 0 20" />
      <path style={{ ...LINE, strokeWidth: 1.75 }} d="M112 22h8l-8 9h8M124 12h6l-6 7h6" />
    </>
  ),
  // A note and sound waves: empty music library.
  music: (
    <>
      <ellipse cx="80" cy="108" rx="40" ry="5" style={SOFT} />
      <path style={LINE} d="M68 86V38l40-8v46" />
      <ellipse cx="60" cy="86" rx="10" ry="8" style={BODY} />
      <ellipse cx="100" cy="78" rx="10" ry="8" style={BODY} />
      <path style={{ ...INK, fill: 'var(--rose-400)' }} d="M68 38l40-8v10l-40 8z" />
      <path style={LINE} d="M124 50c5 5 5 15 0 20M132 44c8 9 8 23 0 32" />
      <path style={LINE} d="M36 50c-5 5-5 15 0 20" />
    </>
  ),
  // An open box with a heart flying in: sent.
  sent: (
    <>
      <ellipse cx="80" cy="108" rx="44" ry="5" style={SOFT} />
      <rect x="44" y="62" width="72" height="42" rx="6" style={BODY} />
      <path style={BODY} d="M44 62l-8-14 44 6 44-6-8 14z" />
      <rect x="74" y="62" width="12" height="42" style={{ ...ACCENT, opacity: 0.85 }} />
      {heart(69, 20, 1.9)}
      <path style={{ ...LINE, strokeDasharray: '2 5' }} d="M80 42v10" />
      <path style={LINE} d="M40 30l4 4M120 30l-4 4M34 44h6M126 44h-6" />
    </>
  ),
  // An envelope with an exclamation mark: send failed.
  failed: (
    <>
      <ellipse cx="80" cy="108" rx="44" ry="5" style={SOFT} />
      <rect x="36" y="48" width="80" height="54" rx="6" style={BODY} />
      <path style={LINE} d="M38 52l38 28 38-28" />
      {heart(64, 76, 1, { ...ACCENT, opacity: 0.5 })}
      <circle cx="118" cy="46" r="16" style={{ ...INK, fill: 'var(--error-fill)', stroke: 'var(--neutral-0)', strokeWidth: 3 }} />
      <path style={{ stroke: '#FFFFFF', strokeWidth: 3, strokeLinecap: 'round' }} d="M118 38v10M118 54v.5" />
    </>
  ),
};

export default function Illustration({ name, width = 160 }) {
  return (
    <svg width={width} height={(width * 120) / 160} viewBox="0 0 160 120" aria-hidden="true" style={{ flex: '0 0 auto' }}>
      {SCENES[name]}
    </svg>
  );
}
