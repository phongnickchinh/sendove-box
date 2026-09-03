import { useNavigate } from 'react-router-dom';
import { useAuth } from '../context/AuthContext';
import { logOut } from '../api/auth';
import Icon from '../components/ui/Icon';
import { Screen, AppBar, Body, Header, CircleIcon } from '../components/ui/Screen';

export default function Dashboard() {
  const { user, profile } = useAuth();
  const navigate = useNavigate();

  const handleLogout = async () => {
    await logOut();
    navigate('/');
  };

  const boxes = Object.entries(profile?.boxes_list || {});
  const firstName = user?.displayName?.split(' ').slice(-1)[0] || 'bạn';

  return (
    <Screen>
      <AppBar
        right={
          <button type="button" className="sl-iconbtn" onClick={handleLogout} aria-label="Đăng xuất">
            <Icon name="power" size={20} />
          </button>
        }
      />
      <Body>
        <Header title={`Chào ${firstName}`} />
        <p className="sl-body">Chọn một hộp quà để tiếp tục.</p>

        {boxes.length === 0 ? (
          <div className="sl-card sl-card--center">
            <CircleIcon size={56} bg="var(--rose-50)" color="var(--rose-400)" icon="chat" iconSize={24} />
            <span className="sl-heading">Chưa có hộp nào</span>
            <span className="sl-body">Ghép đôi chiếc hộp đầu tiên bằng mã hiện trên màn hình của nó.</span>
          </div>
        ) : (
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
                  <Icon name={box.role === 'sender' ? 'chat' : 'heart'} size={24} />
                </span>
                <div className="sl-listcard__mid">
                  <span className="sl-label-s" style={{ fontSize: 15 }}>{box.box_name}</span>
                  <span className="sl-caption">
                    {box.role === 'sender' ? 'Bạn là người gửi' : 'Bạn là người nhận'}
                  </span>
                </div>
                <Icon name="chevron" size={16} style={{ color: 'var(--neutral-400)' }} />
              </button>
            ))}
          </div>
        )}

        <button type="button" className="sl-addrow" onClick={() => navigate('/pair')}>
          <Icon name="plus" size={20} />
          Kết nối hộp mới
        </button>
      </Body>
    </Screen>
  );
}
