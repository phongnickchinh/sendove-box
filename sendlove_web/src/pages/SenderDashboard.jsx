import React, { useEffect, useState } from 'react';
import { useParams, useNavigate } from 'react-router-dom';
import { getMessages } from '../api/message';
import { getBoxDetails } from '../api/box';
import { useAuth } from '../context/AuthContext';
import Icon from '../components/ui/Icon';
import MessageDetail from '../components/MessageDetail';
import UnpairConfirm from '../components/UnpairConfirm';
import { Screen, AppBar, Body, Actions, Header, Button, Tips, CircleIcon } from '../components/ui/Screen';
import { clock, dayLabel, iconOf, kindOf, timeAgo, titleOf } from '../utils/messageFormat';

/**
 * Màn 06 "content-history-below-part" — lịch sử tin đã gửi của một hộp.
 *
 * KHÔNG có badge "đã nhận / đã xem": Message không có trường trạng thái —
 * message.types.ts ghi rõ "Sender không được biết trạng thái tin nhắn", ESP32
 * chỉ so timestamp với last_download_ts nội bộ của nó. Thứ người gửi thật sự
 * biết được là lần hộp thức dậy gần nhất, nên đó là thứ hiện lên.
 *
 * Phân trang: backend chỉ có ?limit (N tin mới nhất, tối đa 100), không có
 * con trỏ — "Xem thêm" là tải lại với limit lớn hơn.
 */

const PAGE = 20;
const MAX_LIMIT = 100; // kẹp ở message.controller.ts getMessages

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

  // Trạng thái hộp chỉ cần đọc một lần, không tải lại mỗi lần "Xem thêm".
  useEffect(() => {
    let alive = true;
    getBoxDetails(boxId)
      .then((res) => { if (alive && res.success) setLastSeen(res.data?.status?.last_seen); })
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

  // Mới nhất lên trước, rồi gom theo ngày để chèn nhãn.
  const sorted = [...messages].sort((a, b) => b.timestamp - a.timestamp);
  const shown = filter === 'all' ? sorted : sorted.filter((m) => kindOf(m) === filter);
  // Trả về đủ số đã xin thì có thể còn tin cũ hơn chưa tải.
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

        <div className="sl-note" style={{ border: '0.5px solid var(--caramel-300)', alignItems: 'center' }}>
          <Icon name="sync" size={16} style={{ color: 'var(--rose-700)' }} />
          <span style={{ fontWeight: 500, color: 'var(--neutral-500)' }}>
            {lastSeen ? `Hộp thức dậy lần cuối ${timeAgo(lastSeen)}` : 'Chưa rõ lần hộp thức gần nhất'}
          </span>
        </div>

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
            <CircleIcon size={56} bg="var(--rose-50)" color="var(--rose-400)" icon="chat" iconSize={24} />
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
                  <button type="button" className="sl-listcard sl-msgrow" style={{ alignItems: 'flex-start' }}
                    onClick={() => setOpenMsg(msg)}>
                    <span className="sl-chip">
                      <Icon name={iconOf(msg)} size={20} />
                    </span>
                    <span className="sl-listcard__mid">
                      <span className="sl-label-s">{titleOf(msg)}</span>
                      <span className="sl-caption" style={{
                        display: '-webkit-box', WebkitLineClamp: 2, WebkitBoxOrient: 'vertical', overflow: 'hidden',
                      }}>
                        {msg.text || 'Không kèm dòng chữ nào'}
                      </span>
                    </span>
                    <span className="sl-caption" style={{ color: 'var(--neutral-400)', flex: '0 0 auto' }}>
                      {clock(msg.timestamp)}
                    </span>
                  </button>
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

        <Tips>Hộp không báo ngược lại, nên không có dấu "đã xem".</Tips>

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
