import React, { useEffect, useState } from 'react';
import { useParams, useNavigate } from 'react-router-dom';
import { getMessages } from '../api/message';
import { getBoxDetails } from '../api/box';
import { useAuth } from '../context/AuthContext';
import Icon from '../components/ui/Icon';
import MessageDetail from '../components/MessageDetail';
import MessageRow from '../components/MessageRow';
import UnpairConfirm from '../components/UnpairConfirm';
import Illustration from '../components/ui/Illustration';
import { Screen, AppBar, Body, Actions, Header, Button, Tips } from '../components/ui/Screen';
import { clock, dayLabel, kindOf, timeAgo } from '../utils/messageFormat';
import { lastSeenMs, syncTone } from '../utils/boxStatus';

/** Same status-dot palette as the Dashboard box cards. */
const TONE_DOT = {
  ok: 'var(--success-fill)',
  late: 'var(--warning-fill)',
  lost: 'var(--error-fill)',
  unknown: 'var(--neutral-400)',
};

/**
 * Sent-message history of one box. There is NO "delivered / seen" badge:
 * messages have no status field by design; the box's last wake-up is shown
 * instead. Pagination: ?limit only (max 100), so "Load more" refetches with a
 * larger limit.
 */

const PAGE = 20;
const MAX_LIMIT = 100; // clamped in message.controller.ts getMessages

const FILTERS = [
  { key: 'all', label: 'Tất cả' },
  { key: 'video', label: 'Video', icon: 'video' },
  { key: 'image', label: 'Ảnh', icon: 'image' },
  { key: 'voice', label: 'Thoại', icon: 'mic' },
  { key: 'text', label: 'Chữ', icon: 'text' },
  { key: 'static', label: 'Tĩnh', icon: 'layers' },
];

export default function SenderDashboard() {
  const { boxId } = useParams();
  const navigate = useNavigate();
  const { profile } = useAuth();
  const [messages, setMessages] = useState([]);
  const [lastSeen, setLastSeen] = useState(null);
  const [limit, setLimit] = useState(PAGE);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState(null);
  const [filter, setFilter] = useState('all');
  const [openMsg, setOpenMsg] = useState(null);
  const [menuOpen, setMenuOpen] = useState(false);
  const [confirmUnpair, setConfirmUnpair] = useState(false);

  // Box status is read once, not on every "Load more".
  useEffect(() => {
    let alive = true;
    getBoxDetails(boxId)
      .then((res) => { if (alive && res.success) setLastSeen(lastSeenMs(res.data?.status)); })
      .catch(() => {});
    return () => { alive = false; };
  }, [boxId]);

  useEffect(() => {
    let alive = true;
    setLoading(true);
    getMessages(boxId, limit)
      .then((res) => {
        if (!alive) return;
        setMessages(res.data || []);
        setError(null);
      })
      .catch(() => { if (alive) setError('Không tải được lịch sử tin nhắn. Kiểm tra kết nối rồi thử lại.'); })
      .finally(() => { if (alive) setLoading(false); });
    return () => { alive = false; };
  }, [boxId, limit]);

  // Newest first, then grouped by day to insert the day labels.
  const sorted = [...messages].sort((a, b) => b.timestamp - a.timestamp);
  const shown = filter === 'all' ? sorted : sorted.filter((m) => kindOf(m) === filter);
  // A full page back means there may be older messages not loaded yet.
  const mayHaveMore = messages.length >= limit && limit < MAX_LIMIT;
  const firstLoad = loading && messages.length === 0;
  let lastDay = null;

  return (
    <Screen>
      <AppBar
        onBack={() => navigate(`/box/${boxId}/sender`)}
        right={
          <button type="button" className="sl-iconbtn" aria-label="Cài đặt hộp" aria-expanded={menuOpen}
            onClick={() => setMenuOpen((v) => !v)}>
            <Icon name="gear" size={22} />
          </button>
        }
      />
      <Body>
        <Header title="Lịch sử tin nhắn" to={profile?.boxes_list?.[boxId]?.box_name || `Hộp ${boxId}`} />

        <span className="sl-statuschip">
          <span className="sl-boxcard__dot" style={{ background: TONE_DOT[syncTone(lastSeen)] }} />
          {lastSeen ? `Hộp thức ${timeAgo(lastSeen)}` : 'Chưa rõ lần hộp thức'}
        </span>

        {error && <div className="sl-reason">{error}</div>}

        {messages.length > 0 && (
          <div className="sl-filters" role="toolbar" aria-label="Lọc theo loại tin">
            {FILTERS.map((f) => (
              <button key={f.key} type="button" className="sl-filter"
                aria-pressed={filter === f.key} onClick={() => setFilter(f.key)}>
                {f.icon && <Icon name={f.icon} size={14} />}
                {f.label}
              </button>
            ))}
          </div>
        )}

        {firstLoad ? (
          <span className="sl-body">Đang tải…</span>
        ) : sorted.length === 0 && !error ? (
          <div className="sl-card sl-card--center">
            <Illustration name="inbox" />
            <span className="sl-heading">Chưa gửi tin nào</span>
            <span className="sl-body">Gửi lời nhắn đầu tiên — hộp sẽ nhận ở lần thức dậy kế tiếp.</span>
          </div>
        ) : shown.length === 0 && sorted.length > 0 ? (
          <div className="sl-card sl-card--center" style={{ padding: 'var(--sp-5) var(--sp-4)' }}>
            <span className="sl-body">
              Không có tin loại này trong {sorted.length} tin gần nhất.
            </span>
            <Button kind="gho" block={false} onClick={() => setFilter('all')}>Xem tất cả</Button>
          </div>
        ) : (
          <div style={{ display: 'flex', flexDirection: 'column', gap: 'var(--sp-3)' }}>
            {shown.map((msg) => {
              const day = dayLabel(msg.timestamp);
              const showDay = day !== lastDay;
              lastDay = day;
              return (
                <React.Fragment key={msg.id}>
                  {showDay && (
                    <span className="sl-caption" style={{ color: 'var(--neutral-400)', marginTop: 'var(--sp-1)' }}>{day}</span>
                  )}
                  <MessageRow msg={msg} meta={clock(msg.timestamp)} noText="Không kèm dòng chữ nào"
                    onOpen={() => setOpenMsg(msg)} />
                </React.Fragment>
              );
            })}
          </div>
        )}

        {mayHaveMore && !firstLoad && (
          <Button kind="gho" disabled={loading} onClick={() => setLimit((l) => Math.min(l + PAGE, MAX_LIMIT))}>
            {loading ? 'Đang tải…' : 'Xem tin cũ hơn'}
          </Button>
        )}

        <Tips>Hộp không báo đã xem.</Tips>

        <Actions>
          <Button kind="pri" onClick={() => navigate(`/box/${boxId}/sender`)}>Gửi tin mới</Button>
        </Actions>
      </Body>

      {menuOpen && (
        <div className="sl-popover-anchor" onClick={() => setMenuOpen(false)}>
          <div className="sl-popover" onClick={(e) => e.stopPropagation()}>
            <button type="button" className="sl-popover__row sl-popover__row--danger"
              onClick={() => { setMenuOpen(false); setConfirmUnpair(true); }}>
              <Icon name="unlink" size={18} />
              Huỷ ghép đôi hộp này
            </button>
          </div>
        </div>
      )}

      {openMsg && <MessageDetail boxId={boxId} message={openMsg} onClose={() => setOpenMsg(null)} />}

      {confirmUnpair && (
        <UnpairConfirm boxId={boxId} role="sender" onClose={() => setConfirmUnpair(false)} onError={setError} />
      )}
    </Screen>
  );
}
