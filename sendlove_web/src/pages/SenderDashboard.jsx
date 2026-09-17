import React, { useEffect, useState } from 'react';
import { useParams, useNavigate } from 'react-router-dom';
import { getMessages } from '../api/message';
import { getBoxDetails } from '../api/box';
import { useAuth } from '../context/AuthContext';
import Icon from '../components/ui/Icon';
import { Screen, AppBar, Body, Actions, Header, Button, Tips, CircleIcon } from '../components/ui/Screen';

/**
 * Màn 06 "content-history-below-part".
 *
 * KHÔNG có badge "đã nhận / đã xem": Message không có trường trạng thái —
 * message.types.ts ghi rõ "Sender không được biết trạng thái tin nhắn", ESP32
 * chỉ so timestamp với last_download_ts nội bộ của nó. Thứ người gửi thật sự
 * biết được là lần hộp thức dậy gần nhất, nên đó là thứ hiện lên.
 */

const MINUTE = 60 * 1000;

function timeAgo(ts) {
  const diff = Date.now() - ts;
  if (diff < MINUTE) return 'vừa xong';
  const mins = Math.floor(diff / MINUTE);
  if (mins < 60) return `${mins} phút trước`;
  const hours = Math.floor(mins / 60);
  if (hours < 24) return `${hours} giờ trước`;
  return `${Math.floor(hours / 24)} ngày trước`;
}

const clock = (ts) => new Date(ts).toLocaleTimeString('vi-VN', { hour: '2-digit', minute: '2-digit' });

/** Nhãn ngày: hôm nay / hôm qua / ngày tháng. */
function dayLabel(ts) {
  const d = new Date(ts);
  const today = new Date();
  const yesterday = new Date(today);
  yesterday.setDate(today.getDate() - 1);
  const same = (a, b) => a.toDateString() === b.toDateString();
  if (same(d, today)) return 'Hôm nay';
  if (same(d, yesterday)) return 'Hôm qua';
  return d.toLocaleDateString('vi-VN', { day: '2-digit', month: '2-digit', year: 'numeric' });
}

const ICON_OF = { video: 'video', image: 'image', gif: 'image', voice: 'mic', text: 'text' };

function titleOf(msg) {
  const secs = msg.duration ? ` · ${Math.round(msg.duration)}s` : '';
  switch (msg.type) {
    case 'video': return `Video${secs}`;
    case 'voice': return `Lời nhắn${secs}`;
    case 'image': return 'Ảnh';
    case 'gif': return 'Ảnh động';
    case 'text': return 'Dòng chữ';
    default: return 'Tin nhắn';
  }
}

export default function SenderDashboard() {
  const { boxId } = useParams();
  const navigate = useNavigate();
  const { profile } = useAuth();
  const [messages, setMessages] = useState([]);
  const [lastSeen, setLastSeen] = useState(null);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState(null);

  useEffect(() => {
    let alive = true;
    (async () => {
      const [msgRes, boxRes] = await Promise.allSettled([getMessages(boxId), getBoxDetails(boxId)]);
      if (!alive) return;
      if (msgRes.status === 'fulfilled' && msgRes.value.success) setMessages(msgRes.value.data || []);
      else setError('Không tải được lịch sử tin nhắn.');
      if (boxRes.status === 'fulfilled' && boxRes.value.success) setLastSeen(boxRes.value.data?.status?.last_seen);
      setLoading(false);
    })();
    return () => { alive = false; };
  }, [boxId]);

  // Mới nhất lên trước, rồi gom theo ngày để chèn nhãn.
  const sorted = [...messages].sort((a, b) => b.timestamp - a.timestamp);
  let lastDay = null;

  return (
    <Screen>
      <AppBar onBack={() => navigate('/dashboard')} />
      <Body>
        <Header title="Lịch sử tin nhắn" to={profile?.boxes_list?.[boxId]?.box_name || `Hộp ${boxId}`} />

        <div className="sl-note" style={{ border: '0.5px solid var(--caramel-300)', alignItems: 'center' }}>
          <Icon name="sync" size={16} style={{ color: 'var(--rose-700)' }} />
          <span style={{ fontWeight: 500, color: 'var(--neutral-500)' }}>
            {lastSeen ? `Hộp thức dậy lần cuối ${timeAgo(lastSeen)}` : 'Chưa rõ lần hộp thức gần nhất'}
          </span>
        </div>

        {error && <div className="sl-reason">{error}</div>}

        <span className="sl-heading" style={{ color: 'var(--neutral-500)' }}>ĐÃ GỬI</span>

        {loading ? (
          <span className="sl-body">Đang tải…</span>
        ) : sorted.length === 0 ? (
          <div className="sl-card sl-card--center">
            <CircleIcon size={56} bg="var(--rose-50)" color="var(--rose-400)" icon="chat" iconSize={24} />
            <span className="sl-heading">Chưa gửi tin nào</span>
            <span className="sl-body">Gửi lời nhắn đầu tiên — hộp sẽ nhận ở lần thức dậy kế tiếp.</span>
          </div>
        ) : (
          <div style={{ display: 'flex', flexDirection: 'column', gap: 'var(--sp-4)' }}>
            {sorted.map((msg) => {
              const day = dayLabel(msg.timestamp);
              const showDay = day !== lastDay;
              lastDay = day;
              return (
                <React.Fragment key={msg.id}>
                  {showDay && (
                    <span className="sl-caption" style={{ color: 'var(--neutral-400)' }}>{day}</span>
                  )}
                  <div className="sl-listcard" style={{ alignItems: 'flex-start' }}>
                    <span className="sl-chip">
                      <Icon name={ICON_OF[msg.type] || 'chat'} size={20} />
                    </span>
                    <div className="sl-listcard__mid">
                      <span className="sl-label-s">{titleOf(msg)}</span>
                      <span className="sl-caption" style={{
                        display: '-webkit-box', WebkitLineClamp: 2, WebkitBoxOrient: 'vertical', overflow: 'hidden',
                      }}>
                        {msg.text || 'Không kèm dòng chữ nào'}
                      </span>
                    </div>
                    <span className="sl-caption" style={{ color: 'var(--neutral-400)', flex: '0 0 auto' }}>
                      {clock(msg.timestamp)}
                    </span>
                  </div>
                </React.Fragment>
              );
            })}
          </div>
        )}

        <Tips>Hộp không báo ngược lại, nên không có dấu "đã xem".</Tips>

        <Actions>
          <Button kind="pri" onClick={() => navigate(`/box/${boxId}/sender`)}>Gửi tin mới</Button>
        </Actions>
      </Body>
    </Screen>
  );
}
