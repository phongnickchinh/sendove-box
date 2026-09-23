import React, { useEffect, useState } from 'react';
import { getMessageDetails } from '../api/message';
import { fullDate, iconOf, isStatic, titleOf } from '../utils/messageFormat';
import Icon from './ui/Icon';
import { Button, Modal } from './ui/Screen';

/**
 * Popup "message-detail-popup" (+ biến thể voice), dùng chung cho người gửi
 * (lịch sử) và người nhận (danh sách tin của hộp).
 *
 * CHƯA đối chiếu với frame Figma thật — dựng theo style guide Warm Minimalism
 * trong lúc Figma MCP hết quota. Cần soi lại khi đọc được frame.
 *
 * Dữ liệu: danh sách chỉ có storage path thô, nên mở popup mới gọi
 * GET /messages/:id để lấy signed URL (hết hạn 15 phút — popup mở lâu hơn thế
 * thì media không tải lại được, đóng mở lại là có URL mới).
 */
export default function MessageDetail({ boxId, message, onClose }) {
  const [detail, setDetail] = useState(null);
  const [error, setError] = useState(null);

  useEffect(() => {
    let alive = true;
    getMessageDetails(boxId, message.id)
      .then((res) => { if (alive) setDetail(res.data); })
      .catch((err) => {
        if (alive) setError(err.response?.data?.error?.message || 'Không mở được tin nhắn này.');
      });
    return () => { alive = false; };
  }, [boxId, message.id]);

  const msg = detail || message;
  const media = detail?.media || {};
  const kindIsText = msg.type === 'text';
  const hasNote = !kindIsText && !!msg.text;

  return (
    <Modal onClose={onClose} className="sl-msgdetail">
      <div className="sl-msgdetail__head">
        <span className="sl-chip"><Icon name={iconOf(msg)} size={20} /></span>
        <div className="sl-listcard__mid">
          <span className="sl-heading">{titleOf(msg)}</span>
          <span className="sl-caption">{fullDate(msg.timestamp)}</span>
        </div>
        <button type="button" className="sl-iconbtn" onClick={onClose} aria-label="Đóng">
          <Icon name="x" size={22} />
        </button>
      </div>

      {error && <div className="sl-reason">{error}</div>}

      {!detail && !error && !kindIsText && (
        <div className="sl-msgdetail__media sl-msgdetail__media--loading" aria-busy="true">
          <span className="sl-caption">Đang tải…</span>
        </div>
      )}

      {detail && <Media msg={msg} media={media} />}

      {kindIsText && (
        <p className="sl-msgdetail__text">{msg.text}</p>
      )}

      {hasNote && (
        <div className="sl-msgdetail__note">
          <span className="sl-label-s">Lời nhắn kèm</span>
          <p className="sl-body" style={{ margin: 0, whiteSpace: 'pre-wrap' }}>{msg.text}</p>
        </div>
      )}

      <Button kind="gho" onClick={onClose}>Đóng</Button>
    </Modal>
  );
}

function Media({ msg, media }) {
  if (msg.type === 'video') {
    if (media.video) {
      return (
        <div className="sl-msgdetail__media">
          <video src={media.video} poster={media.thumbnail} controls playsInline preload="metadata" />
        </div>
      );
    }
    return <Fallback media={media} what="video" />;
  }

  if (msg.type === 'image' || msg.type === 'gif') {
    const src = media.image || media.thumbnail;
    return (
      <>
        {src ? (
          <div className="sl-msgdetail__media"><img src={src} alt="" /></div>
        ) : !isStatic(msg) && <Fallback media={media} what="ảnh" />}
        {media.bg_music && <AudioRow label="Nhạc nền" src={media.bg_music} />}
      </>
    );
  }

  if (msg.type === 'voice') {
    return media.voice
      ? <AudioRow src={media.voice} big />
      : <Fallback media={media} what="đoạn ghi âm" />;
  }

  return null;
}

/** label bỏ trống khi tiêu đề popup đã nói rõ đây là gì (tin thoại). */
function AudioRow({ label, src, big = false }) {
  return (
    <div className={`sl-msgdetail__audio${big ? ' sl-msgdetail__audio--big' : ''}`}>
      {label && (
        <span style={{ display: 'flex', alignItems: 'center', gap: 'var(--sp-2)' }}>
          <Icon name="mic" size={16} style={{ color: 'var(--text-accent)' }} />
          <span className="sl-label-s">{label}</span>
        </span>
      )}
      <audio src={src} controls preload="metadata" />
    </div>
  );
}

/** File gốc không có hoặc không ký được: nói thẳng, vẫn hiện thumbnail nếu có. */
function Fallback({ media, what }) {
  return (
    <>
      {media.thumbnail && (
        <div className="sl-msgdetail__media"><img src={media.thumbnail} alt="" /></div>
      )}
      <div className="sl-note">
        <Icon name="info" size={16} />
        <span>Không mở được {what} gốc trên web. Hộp vẫn phát bản đã gửi xuống.</span>
      </div>
    </>
  );
}
