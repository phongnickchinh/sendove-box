import React, { useState } from 'react';
import Icon from './ui/Icon';
import { iconOf, titleOf } from '../utils/messageFormat';

/**
 * Một dòng tin nhắn — dùng chung cho lịch sử người gửi và danh sách người nhận.
 *
 * msg.thumbnail: signed URL (15 phút) backend gắn sẵn trong GET /messages. Không có
 * (tin chữ/thoại, backend chưa deploy) hoặc ảnh hỏng (URL hết hạn) thì về icon như cũ.
 */
export default function MessageRow({ msg, meta, noText = null, onOpen }) {
  const [broken, setBroken] = useState(false);
  const thumb = msg.thumbnail && !broken;
  const text = msg.text || noText;

  return (
    <button type="button" className="sl-listcard sl-msgrow" style={{ alignItems: 'flex-start' }} onClick={onOpen}>
      {thumb ? (
        <span className="sl-chip sl-thumb">
          <img src={msg.thumbnail} alt="" loading="lazy" onError={() => setBroken(true)} />
          {msg.type === 'video' && <span className="sl-thumb__badge"><Icon name="play" size={10} /></span>}
        </span>
      ) : (
        <span className="sl-chip sl-chip--muted">
          <Icon name={iconOf(msg)} size={20} />
        </span>
      )}
      <span className="sl-listcard__mid">
        <span className="sl-label-s">{titleOf(msg)}</span>
        {text && (
          <span className="sl-caption" style={{
            display: '-webkit-box', WebkitLineClamp: 2, WebkitBoxOrient: 'vertical', overflow: 'hidden',
          }}>
            {text}
          </span>
        )}
      </span>
      <span className="sl-caption" style={{ color: 'var(--neutral-400)', flex: '0 0 auto' }}>{meta}</span>
    </button>
  );
}
