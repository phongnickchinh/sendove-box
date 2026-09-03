import { useState } from 'react';
import { useNavigate } from 'react-router-dom';
import { useAuth } from '../context/AuthContext';
import { useTheme } from '../context/ThemeContext';
import { logOut } from '../api/auth';
import Icon from '../components/ui/Icon';
import { Screen, AppBar, Body, CircleIcon } from '../components/ui/Screen';

const ROLE_LABEL = { sender: 'Người gửi', receiver: 'Người nhận' };

/** Ảnh đại diện 44px viền, hoặc chữ cái đầu tên nếu không có ảnh. */
function Avatar({ user }) {
  if (user?.photoURL) return <span className="sl-avatar"><img src={user.photoURL} alt="" /></span>;
  const initial = (user?.displayName || '?').trim().charAt(0).toUpperCase();
  return <span className="sl-avatar">{initial}</span>;
}

export default function Dashboard() {
  const { user, profile } = useAuth();
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
        {boxes.length === 0 ? (
          <>
            <span className="sl-section-label">Hộp của bạn</span>
            <p className="sl-body" style={{ margin: 0 }}>
              Kết nối một hộp mới để bắt đầu gửi tin nhắn.
            </p>
          </>
        ) : (
          <>
            <span className="sl-section-label">Hộp của bạn</span>
            <div style={{ display: 'flex', flexDirection: 'column', gap: 10 }}>
              {boxes.map(([boxId, box]) => (
                <button
                  key={boxId}
                  type="button"
                  className="sl-listcard"
                  style={{ cursor: 'pointer' }}
                  onClick={() => navigate(`/box/${boxId}/${box.role === 'sender' ? 'sender' : 'receiver'}`)}
                >
                  <span className="sl-chip">
                    <Icon name={box.role === 'sender' ? 'chat' : 'mailbox'} size={24} />
                  </span>
                  <div className="sl-listcard__mid">
                    <span className="sl-label-s" style={{ fontSize: 15 }}>{box.box_name}</span>
                    <span className={`sl-rolepill sl-rolepill--${box.role === 'sender' ? 'sender' : 'receiver'}`}>
                      {ROLE_LABEL[box.role] || ROLE_LABEL.receiver}
                    </span>
                  </div>
                  <Icon name="chevron" size={16} style={{ color: 'var(--neutral-400)' }} />
                </button>
              ))}
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
                  type="button" className="sl-toggle" role="switch"
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
