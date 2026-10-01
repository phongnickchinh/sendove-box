import React, { useEffect, useState } from 'react';
import { useParams, useNavigate } from 'react-router-dom';
import { getMessages } from '../api/message';
import { getBoxDetails } from '../api/box';
import { useAuth } from '../context/AuthContext';
import Icon from '../components/ui/Icon';
import MessageDetail from '../components/MessageDetail';
import MessageRow from '../components/MessageRow';
import Illustration from '../components/ui/Illustration';
import { Screen, AppBar, Body, Header, CircleIcon } from '../components/ui/Screen';
import { timeAgo } from '../utils/messageFormat';
import { fwVersion, lastSeenMs, syncTone } from '../utils/boxStatus';

/**
 * Box status + received messages (receiver home).
 *
 * Two BoxStatus fields are INTENTIONALLY not shown:
 *   online   — the box sleeps and wakes only every 5 minutes, so online = false
 *              nearly all the time while the box is fine; showing it would be
 *              a false alarm. Status here is derived from last_seen.
 *   charging — PowerManager::isCharging() is hardcoded to return false (there
 *              is no charge-detect circuit). Only the battery % is shown.
 */


const TONE = {
  ok:      { bg: 'var(--success-bg)', fg: 'var(--success-fill)', icon: 'sync' },
  late:    { bg: 'var(--warning-bg)', fg: 'var(--warning-fill)', icon: 'sync' },
  lost:    { bg: 'var(--error-bg)',   fg: 'var(--error-fill)',   icon: 'alert' },
  unknown: { bg: 'var(--neutral-100)', fg: 'var(--neutral-500)', icon: 'sync' },
};


export default function ReceiverUI() {
  const { boxId } = useParams();
  const navigate = useNavigate();
  const { profile } = useAuth();
  const [messages, setMessages] = useState([]);
  const [box, setBox] = useState(null);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState(null);
  const [openMsg, setOpenMsg] = useState(null);

  useEffect(() => {
    let alive = true;
    const load = async () => {
      try {
        // The two calls are independent: if messages fail the box status is
        // still shown and vice versa — hence allSettled, not all.
        const [msgRes, boxRes] = await Promise.allSettled([
          getMessages(boxId),
          getBoxDetails(boxId),
        ]);
        if (!alive) return;
        if (msgRes.status === 'fulfilled' && msgRes.value.success) setMessages(msgRes.value.data);
        if (boxRes.status === 'fulfilled' && boxRes.value.success) setBox(boxRes.value.data);
        if (msgRes.status === 'rejected' && boxRes.status === 'rejected') {
          setError('Không đọc được dữ liệu hộp. Kiểm tra kết nối rồi thử lại.');
        } else if (msgRes.status === 'rejected') {
          // Without an error, an empty list looks just like "no messages yet".
          setError('Không tải được danh sách tin nhắn.');
        }
      } finally {
        if (alive) setLoading(false);
      }
    };
    load();
    return () => { alive = false; };
  }, [boxId]);

  const status = box?.status;
  const seenAt = lastSeenMs(status);
  const tone = TONE[syncTone(seenAt)];

  return (
    <Screen>
      <AppBar onBack={() => navigate('/dashboard')} />
      <Body>
        <Header title="Hộp của tôi" to={profile?.boxes_list?.[boxId]?.box_name || `Hộp ${boxId}`} />

        {/* --- sync card --- */}
        <div className="sl-card" style={{ gap: 'var(--sp-3)', padding: 'var(--sp-4)' }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: 'var(--sp-3)' }}>
            <CircleIcon size={44} bg={tone.bg} color={tone.fg} icon={tone.icon} iconSize={20} />
            <div className="sl-listcard__mid">
              <span className="sl-label-s" style={{ fontSize: 15 }}>
                {seenAt
                  ? `Đồng bộ ${timeAgo(seenAt)}`
                  : loading ? 'Đang đọc trạng thái…' : 'Chưa rõ lần đồng bộ gần nhất'}
              </span>
            </div>
          </div>

          {status && (
            <div style={{ display: 'flex', alignItems: 'center', gap: 'var(--sp-4)' }}>
              <span style={{ display: 'flex', alignItems: 'center', gap: 6, color: 'var(--neutral-500)' }}>
                <Icon name="battery" size={16} />
                <span className="sl-caption" style={{ fontWeight: 500, color: 'var(--caramel-800)' }}>
                  {status.battery}%
                </span>
              </span>
              <span style={{ display: 'flex', alignItems: 'center', gap: 6, color: 'var(--neutral-500)' }}>
                <Icon name="gear" size={16} />
                <span className="sl-caption" style={{ fontWeight: 500, color: 'var(--caramel-800)' }}>
                  Firmware {fwVersion(status) || '—'}
                </span>
              </span>
            </div>
          )}
        </div>

        {error && <div className="sl-reason">{error}</div>}

        {/* --- navigation tiles --- */}
        <div style={{ display: 'flex', gap: 'var(--sp-3)' }}>
          <NavTile icon="bell" label="Báo thức" onClick={() => navigate(`/box/${boxId}/receiver/alarm`)} />
          <NavTile icon="gear" label="Cài đặt" onClick={() => navigate(`/box/${boxId}/receiver/config`)} />
        </div>

        {/* --- received messages --- */}
        <span className="sl-label">Tin nhắn</span>

        {loading ? (
          <span className="sl-body">Đang tải…</span>
        ) : messages.length === 0 ? (
          <div className="sl-card sl-card--center">
            <Illustration name="inbox" />
            <span className="sl-heading">Chưa có tin nào</span>
            <span className="sl-body">Khi người ấy gửi, tin sẽ hiện ở đây rồi mới tới hộp.</span>
          </div>
        ) : (
          <div style={{ display: 'flex', flexDirection: 'column', gap: 10 }}>
            {[...messages].sort((a, b) => b.timestamp - a.timestamp).map((msg) => (
              <MessageRow key={msg.id} msg={msg} meta={timeAgo(msg.timestamp)} onOpen={() => setOpenMsg(msg)} />
            ))}
          </div>
        )}
      </Body>

      {openMsg && <MessageDetail boxId={boxId} message={openMsg} onClose={() => setOpenMsg(null)} />}
    </Screen>
  );
}

/** Navigation tile, 64 tall with a hairline border. */
function NavTile({ icon, label, onClick }) {
  return (
    <button type="button" className="sl-listcard" onClick={onClick}
      style={{ height: 64, padding: '0 var(--sp-3)', gap: 10, cursor: 'pointer', flex: 1 }}>
      <Icon name={icon} size={20} style={{ color: 'var(--chip-fg)' }} />
      <span className="sl-label-s" style={{ flex: 1, textAlign: 'left' }}>{label}</span>
      <Icon name="chevron" size={16} style={{ color: 'var(--neutral-400)' }} />
    </button>
  );
}
