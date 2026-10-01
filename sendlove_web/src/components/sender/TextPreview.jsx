import React from 'react';

/**
 * Previews a text message the way the box draws it (DisplayDriver::showWrappedText
 * + MediaPlayer): black background, white FreeSansBold 9pt, centered from the
 * top, 8px margin, 22px line height, at most 10 lines, then "...". The box
 * strips Vietnamese diacritics before drawing (asciiFoldVietnamese) — the
 * preview strips them the same way so the sender sees what the box will show.
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
