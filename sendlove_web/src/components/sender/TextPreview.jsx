import React from 'react';

/**
 * Previews a text message the way the box draws it (showWrappedText): white on
 * black, 22px lines, at most 10, then "...", with Vietnamese diacritics stripped.
 */
const fold = (s) => s.normalize('NFD').replace(/[̀-ͯ]/g, '').replace(/đ/g, 'd').replace(/Đ/g, 'D');

export default function TextPreview({ text }) {
  const shown = fold(text).replace(/\s+/g, ' ').trim();
  return (
    <div className="sl-textpreview" aria-label="Xem trước trên màn hộp">
      <p className="sl-textpreview__text" style={shown ? undefined : { opacity: 0.4 }}>{shown || 'Chu se hien o day'}</p>
    </div>
  );
}
