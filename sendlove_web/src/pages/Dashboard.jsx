import { useEffect, useState } from 'react';
import { useNavigate } from 'react-router-dom';
import { useAuth } from '../context/AuthContext';
import { useTheme } from '../context/ThemeContext';
import { logOut } from '../api/auth';
import { getBoxDetails } from '../api/box';
import Icon from '../components/ui/Icon';
import InstallHint from '../components/InstallHint';
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
 * Status row under the box name: colored dot + last sync, then battery %.
 * Derived from last_seen, not status.online (the box sleeps almost all the time).
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

/** 44px bordered avatar, or the name's initial when there is no photo. */
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
  const [accountOpen, setAccountOpen] = useState(false);
  const handleLogout = async () => {
    await logOut();
    navigate('/');
  };

  const boxes = Object.entries(profile?.boxes_list || {});
  const boxIds = boxes.map(([id]) => id).join(',');

  // boxId → status (undefined = loading, null = failed to load)
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

  return (
    <Screen>
      <AppBar
        title={`Chào, ${firstName}!`}
        subtitle="Chọn hộp và gửi tin nhắn"
        /* The notification bell stays hidden until there is a real notification source. */
        right={
          <button
            type="button" onClick={() => setAccountOpen((v) => !v)}
            style={{ border: 'none', background: 'none', padding: 0, cursor: 'pointer' }}
            aria-label="Tài khoản" aria-expanded={accountOpen}
          >
            <Avatar user={user} />
          </button>
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
            {/* caramel-700 per Figma (#83513E) — this sentence only, not
                .sl-body's default neutral-500. */}
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
                    {/* Left group: avatar + name + status row. */}
                    <div className="sl-boxcard__left">
                      <span className={`sl-boxcard__avatar sl-boxcard__avatar--${role}`}>
                        <Icon name={role === 'sender' ? 'chat' : 'mailbox'} size={24} />
                      </span>
                      <div className="sl-boxcard__mid">
                        <span style={{ fontSize: 17, fontWeight: 600, lineHeight: 1.3, color: 'var(--caramel-800)' }}>
                          {box.box_name}
                        </span>
                        {/* Real status, fetched per box (GET /boxes/:id — users
                            usually have 1-3 boxes). Never show placeholder values here. */}
                        <BoxStatusRow status={statuses[boxId]} />
                      </div>
                    </div>

                    {/* Right group: role pill + chevron as ONE block, pushed right. */}
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

        <InstallHint card />
      </Body>

      {accountOpen && (
        <div className="sl-popover-anchor" onClick={() => setAccountOpen(false)}>
          {/* The "Settings" entry stays hidden until its page exists. */}
          <div className="sl-popover" onClick={(e) => e.stopPropagation()}>
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
        </div>
      )}
    </Screen>
  );
}
