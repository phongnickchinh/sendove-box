import React, { useEffect, useState } from 'react';
import { useParams, useNavigate } from 'react-router-dom';
import { getMessages } from '../api/message';
import { getBoxDetails } from '../api/box';
import { useAuth } from '../context/AuthContext';
import Icon from '../components/ui/Icon';
import MessageDetail from '../components/MessageDetail';
import { Screen, AppBar, Body, Header, CircleIcon } from '../components/ui/Screen';
import { iconOf, timeAgo, titleOf } from '../utils/messageFormat';
import { fwVersion, lastSeenMs } from '../utils/boxStatus';

/**
 * Màn 09 "box status + history-part" trong file Figma.
 *
 * Hai trường có trong BoxStatus nhưng CỐ Ý không hiện:
 *   online   — hộp ngủ và chỉ thức 5 phút một lần, nên online = false gần như
 *              suốt thời gian hộp vẫn khoẻ. Hiện nó ra là báo hỏng nhầm.
 *              Trạng thái ở đây suy từ last_seen.
 *   charging — PowerManager::isCharging() hardcode return false, không có mạch
 *              báo sạc. Chỉ hiện phần trăm pin.
 */

const MINUTE = 60 * 1000;

/**
 * Hộp thức mỗi 5 phút. Trễ tới 15 phút vẫn là bình thường (lỡ một hai nhịp);
 * quá 2 tiếng thì mới đáng gọi là mất liên lạc.
 */
function syncTone(lastSeen) {
  if (!lastSeen) return 'unknown';
  const diff = Date.now() - lastSeen;
  if (diff < 15 * MINUTE) return 'ok';
  if (diff < 2 * 60 * MINUTE) return 'late';
  return 'lost';
}

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
        // Hai lời gọi độc lập nhau: tin nhắn hỏng thì vẫn xem được trạng thái
        // hộp và ngược lại, nên allSettled chứ không phải all.
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
          // Không báo thì danh sách rỗng trông y như "chưa có tin nào".
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

        {/* --- thẻ đồng bộ --- */}
        <div className="sl-card" style={{ gap: 'var(--sp-3)', padding: 'var(--sp-4)' }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: 'var(--sp-3)' }}>
            <CircleIcon size={44} bg={tone.bg} color={tone.fg} icon={tone.icon} iconSize={20} />
            <div className="sl-listcard__mid">
              <span className="sl-label-s" style={{ fontSize: 15 }}>
                {seenAt
                  ? `Đồng bộ ${timeAgo(seenAt)}`
                  : loading ? 'Đang đọc trạng thái…' : 'Chưa rõ lần đồng bộ gần nhất'}
              </span>
              <span className="sl-caption">Hộp thức dậy mỗi 5 phút để tìm tin mới.</span>
            </div>
          </div>

          {status && (
            <>
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
              {/* Con số trên là của lần hộp thức gần nhất, không phải đo trực tiếp. */}
              <span className="sl-caption" style={{ color: 'var(--neutral-400)' }}>
                Ghi nhận ở lần đồng bộ đó — không phải số đo ngay lúc này.
              </span>
            </>
          )}
        </div>

        {error && <div className="sl-reason">{error}</div>}

        {/* --- hai lối đi --- */}
        <div style={{ display: 'flex', gap: 'var(--sp-3)' }}>
          <NavTile icon="bell" label="Báo thức" onClick={() => navigate(`/box/${boxId}/receiver/alarm`)} />
          <NavTile icon="gear" label="Cài đặt" onClick={() => navigate(`/box/${boxId}/receiver/config`)} />
        </div>

        {/* --- tin đã nhận --- */}
        <span className="sl-label">Tin nhắn</span>

        {loading ? (
          <span className="sl-body">Đang tải…</span>
        ) : messages.length === 0 ? (
          <div className="sl-card sl-card--center">
            <CircleIcon size={56} bg="var(--rose-50)" color="var(--rose-400)" icon="chat" iconSize={24} />
            <span className="sl-heading">Chưa có tin nào</span>
            <span className="sl-body">Khi người ấy gửi, tin sẽ hiện ở đây rồi mới tới hộp.</span>
          </div>
        ) : (
          <div style={{ display: 'flex', flexDirection: 'column', gap: 10 }}>
            {[...messages].sort((a, b) => b.timestamp - a.timestamp).map((msg) => (
              <button type="button" className="sl-listcard sl-msgrow" key={msg.id} onClick={() => setOpenMsg(msg)}>
                <span className="sl-chip">
                  <Icon name={iconOf(msg)} size={20} />
                </span>
                <span className="sl-listcard__mid">
                  <span className="sl-label-s">{titleOf(msg)}</span>
                  <span className="sl-caption">{timeAgo(msg.timestamp)}</span>
                </span>
                <Icon name="chevron" size={16} style={{ color: 'var(--neutral-400)' }} />
              </button>
            ))}
          </div>
        )}
      </Body>

      {openMsg && <MessageDetail boxId={boxId} message={openMsg} onClose={() => setOpenMsg(null)} />}
    </Screen>
  );
}

/** Ô điều hướng cao 64, viền mảnh — tile() ở màn 09. */
function NavTile({ icon, label, onClick }) {
  return (
    <button type="button" className="sl-listcard" onClick={onClick}
      style={{ height: 64, padding: '0 var(--sp-3)', gap: 10, cursor: 'pointer', flex: 1 }}>
      <Icon name={icon} size={20} style={{ color: 'var(--rose-800)' }} />
      <span className="sl-label-s" style={{ flex: 1, textAlign: 'left' }}>{label}</span>
      <Icon name="chevron" size={16} style={{ color: 'var(--neutral-400)' }} />
    </button>
  );
}
