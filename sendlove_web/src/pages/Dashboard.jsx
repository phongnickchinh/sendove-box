import { useEffect, useState } from 'react';
import { useNavigate } from 'react-router-dom';
import { useAuth } from '../context/AuthContext';
import { useTheme } from '../context/ThemeContext';
import { logOut } from '../api/auth';
import { getBoxDetails } from '../api/box';
import Icon from '../components/ui/Icon';
import { Screen, AppBar, Body, Button, CircleIcon } from '../components/ui/Screen';
import { lastSeenMs, syncTone } from '../utils/boxStatus';
import { timeAgo } from '../utils/messageFormat';

const ROLE_LABEL = { sender: 'Người gửi', receiver: 'Người nhận' };

const TONE_DOT = {
  ok: 'var(--success-fill)',
  late: 'var(--warning-fill)',
  lost: 'var(--error-fill)',
  unknown: 'var(--neutral-400)',
};

/**
 * Hàng trạng thái dưới tên hộp: chấm màu + lần đồng bộ gần nhất, rồi % pin.
 * Suy từ last_seen chứ không từ status.online (hộp ngủ gần như suốt).
 */
function BoxStatusRow({ status }) {
  if (status === undefined) {
    return <div className="sl-boxcard__status"><span className="sl-boxcard__statusitem">Đang đọc…</span></div>;
  }
  const seenAt = lastSeenMs(status);
  const tone = syncTone(seenAt);
  return (
    <div className="sl-boxcard__status">
      <span className="sl-boxcard__statusitem">
        <span className="sl-boxcard__dot" style={{ background: TONE_DOT[tone] }} />
        {status === null ? 'Không đọc được' : seenAt ? timeAgo(seenAt) : 'Chưa đồng bộ'}
      </span>
      {typeof status?.battery === 'number' && (
        <span className="sl-boxcard__statusitem">
          <Icon name="battery" size={13} />
          {status.battery}%
        </span>
      )}
    </div>
  );
}

/** Ảnh đại diện 44px viền, hoặc chữ cái đầu tên nếu không có ảnh. */
function Avatar({ user }) {
  if (user?.photoURL) return <span className="sl-avatar"><img src={user.photoURL} alt="" /></span>;
  const initial = (user?.displayName || '?').trim().charAt(0).toUpperCase();
  return <span className="sl-avatar">{initial}</span>;
}

export default function Dashboard() {
  const { user, profile, profileError, refreshProfile } = useAuth();
  const [retrying, setRetrying] = useState(false);
  const retryProfile = async () => {
    setRetrying(true);
    await refreshProfile();
    setRetrying(false);
  };
  const { theme, toggleTheme } = useTheme();
  const navigate = useNavigate();
  const [notifOpen, setNotifOpen] = useState(false);
  const [accountOpen, setAccountOpen] = useState(false);

  const closeAll = () => { setNotifOpen(false); setAccountOpen(false); };
  const handleLogout = async () => {
    await logOut();
    navigate('/');
  };

  const boxes = Object.entries(profile?.boxes_list || {});
  const boxIds = boxes.map(([id]) => id).join(',');

  // boxId → status (undefined = đang đọc, null = không đọc được)
  const [statuses, setStatuses] = useState({});
  useEffect(() => {
    if (!boxIds) return undefined;
    let alive = true;
    const ids = boxIds.split(',');
    Promise.allSettled(ids.map((id) => getBoxDetails(id))).then((results) => {
      if (!alive) return;
      const next = {};
      results.forEach((r, i) => {
        next[ids[i]] = r.status === 'fulfilled' && r.value.success ? (r.value.data?.status || {}) : null;
      });
      setStatuses(next);
    });
    return () => { alive = false; };
  }, [boxIds]);
  const firstName = user?.displayName?.split(' ').slice(-1)[0] || 'bạn';
  const popoverOpen = notifOpen || accountOpen;

  return (
    <Screen>
      <AppBar
        title={`Chào, ${firstName}!`}
        subtitle="Chọn hộp và gửi tin nhắn"
        right={
          <div style={{ display: 'flex', alignItems: 'center', gap: 'var(--sp-5)' }}>
            <button
              type="button" className="sl-iconbtn" aria-label="Thông báo"
              onClick={() => { setNotifOpen((v) => !v); setAccountOpen(false); }}
            >
              <Icon name="bell" size={20} style={{ color: 'var(--rose-500)' }} />
            </button>
            <button
              type="button" onClick={() => { setAccountOpen((v) => !v); setNotifOpen(false); }}
              style={{ border: 'none', background: 'none', padding: 0, cursor: 'pointer' }}
              aria-label="Tài khoản"
            >
              <Avatar user={user} />
            </button>
          </div>
        }
      />
      <Body>
        {profileError && boxes.length === 0 ? (
          <div className="sl-card sl-card--center">
            <CircleIcon size={56} bg="var(--error-bg)" color="var(--error-fill)" icon="alert" iconSize={24} />
            <span className="sl-heading">Chưa tải được danh sách hộp</span>
            <span className="sl-body">Kiểm tra kết nối mạng rồi thử lại. Các hộp đã ghép vẫn còn nguyên.</span>
            <Button kind="gho" block={false} onClick={retryProfile} disabled={retrying}>
              {retrying ? 'Đang thử lại…' : 'Thử lại'}
            </Button>
          </div>
        ) : boxes.length === 0 ? (
          <>
            <span className="sl-section-label">Hộp của bạn</span>
            {/* Màu caramel-700 xác nhận từ Figma (#83513E) — riêng câu này,
                không dùng màu neutral-500 mặc định của .sl-body. */}
            <p className="sl-body" style={{ margin: 0, color: 'var(--caramel-700)' }}>
              Kết nối một hộp mới để bắt đầu gửi tin nhắn.
            </p>
          </>
        ) : (
          <>
            <span className="sl-section-label">Hộp của bạn</span>
            <div style={{ display: 'flex', flexDirection: 'column', gap: 10 }}>
              {boxes.map(([boxId, box]) => {
                const role = box.role === 'sender' ? 'sender' : 'receiver';
                return (
                  <button
                    key={boxId}
                    type="button"
                    className={`sl-boxcard sl-boxcard--${role}`}
                    onClick={() => navigate(`/box/${boxId}/${role}`)}
                  >
                    {/* Nhóm trái: avatar + tên + hàng trạng thái. */}
                    <div className="sl-boxcard__left">
                      <span className={`sl-boxcard__avatar sl-boxcard__avatar--${role}`}>
                        <Icon name={role === 'sender' ? 'chat' : 'mailbox'} size={24} />
                      </span>
                      <div className="sl-boxcard__mid">
                        <span style={{ fontSize: 17, fontWeight: 600, lineHeight: 1.3, color: 'var(--caramel-800)' }}>
                          {box.box_name}
                        </span>
                        {/* Trạng thái thật, đọc riêng từng hộp (GET /boxes/:id — người
                            dùng thường chỉ có 1-3 hộp). Trước đây viết cứng "Online /
                            100%" cho mọi hộp — đúng loại dữ liệu giả không được hiện. */}
                        <BoxStatusRow status={statuses[boxId]} />
                      </div>
                    </div>

                    {/* Nhóm phải: pill vai trò + chevron GỘP CHUNG, neo phải. */}
                    <div className="sl-boxcard__right">
                      <span className={`sl-rolepill sl-rolepill--${role}`}>
                        {ROLE_LABEL[box.role] || ROLE_LABEL.receiver}
                      </span>
                      <Icon name="chevron" size={20} className={`sl-boxcard__chevron--${role}`} />
                    </div>
                  </button>
                );
              })}
            </div>
          </>
        )}

        <button type="button" className="sl-addbox" onClick={() => navigate('/pair')}>
          <span className="sl-addbox__dot"><Icon name="plus" size={24} /></span>
          <span className="sl-addbox__label">Hộp mới</span>
        </button>
      </Body>

      {popoverOpen && (
        <div className="sl-popover-anchor" onClick={closeAll}>
          {notifOpen && (
            <div className="sl-popover" onClick={(e) => e.stopPropagation()}>
              <div style={{ display: 'flex', flexDirection: 'column', alignItems: 'center', gap: 6, padding: '8px 0' }}>
                <CircleIcon size={40} bg="var(--rose-50)" color="var(--rose-400)" icon="bell" iconSize={18} />
                <span className="sl-caption">Chưa có thông báo nào.</span>
              </div>
            </div>
          )}
          {accountOpen && (
            <div className="sl-popover" onClick={(e) => e.stopPropagation()}>
              {/* Chưa có trang cài đặt nào để trỏ tới — hiện nhưng vô hiệu hoá,
                  không tự bịa một route trống. */}
              <div className="sl-popover__row sl-popover__row--muted">
                <Icon name="gear" size={20} />
                Cài đặt
              </div>
              <div className="sl-popover__row">
                <Icon name="palette" size={21} style={{ flex: '0 0 auto' }} />
                <span style={{ flex: 1 }}>Giao diện tối</span>
                <button
                  type="button" className="sl-toggle sl-toggle--accent" role="switch"
                  aria-checked={theme === 'dark'} aria-label="Bật giao diện tối"
                  onClick={toggleTheme}
                >
                  <span className="sl-toggle__knob" />
                </button>
              </div>
              <hr className="sl-popover__divider" />
              <button type="button" className="sl-popover__row sl-popover__row--rose" onClick={handleLogout}>
                <Icon name="power" size={16} />
                Đăng xuất
              </button>
            </div>
          )}
        </div>
      )}
    </Screen>
  );
}
