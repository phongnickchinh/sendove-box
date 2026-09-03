import { useState } from 'react';
import { useNavigate } from 'react-router-dom';
import apiClient from '../api/client';
import { useAuth } from '../context/AuthContext';
import Icon from '../components/ui/Icon';
import { Screen, AppBar, Body, Actions, Header, Button, Tips } from '../components/ui/Screen';

export default function PairBox() {
  const [pairingCode, setPairingCode] = useState('');
  const [boxName, setBoxName] = useState('');
  const [error, setError] = useState('');
  const [isLoading, setIsLoading] = useState(false);

  const navigate = useNavigate();
  const { refreshProfile } = useAuth();

  const handlePair = async (e) => {
    e.preventDefault();
    if (!pairingCode.trim()) { setError('Vui lòng nhập mã kết nối'); return; }
    if (!boxName.trim()) { setError('Vui lòng đặt tên cho hộp quà của bạn'); return; }

    setIsLoading(true);
    setError('');
    try {
      const res = await apiClient.post('/boxes/pair', {
        pairingCode: pairingCode.trim().toUpperCase(),
        boxName: boxName.trim(),
      });
      if (res.data.success) {
        // Tải lại profile để danh sách box ở Dashboard có hộp vừa ghép.
        await refreshProfile();
        navigate('/dashboard');
      }
    } catch (err) {
      setError(
        err.response?.data?.error?.message ||
        err.response?.data?.message ||
        'Có lỗi xảy ra khi kết nối Box. Vui lòng thử lại.'
      );
    } finally {
      setIsLoading(false);
    }
  };

  return (
    <Screen>
      <AppBar onBack={() => navigate('/dashboard')} />
      <form onSubmit={handlePair} style={{ display: 'contents' }}>
        <Body>
          <Header title="Kết nối hộp mới" />
          <p className="sl-body">
            Nhập mã hiện trên màn hình hộp. Mã bắt đầu bằng <b>S</b> nếu bạn là người gửi,
            hoặc <b>R</b> nếu bạn là người nhận.
          </p>

          {error && <div className="sl-reason">{error}</div>}

          <label className="sl-field">
            <span className="sl-label" style={{ display: 'flex', alignItems: 'center', gap: 'var(--sp-2)' }}>
              <Icon name="key" size={18} style={{ color: 'var(--rose-700)' }} />
              Mã kết nối
            </span>
            <input
              className="sl-input"
              value={pairingCode}
              onChange={(e) => setPairingCode(e.target.value.toUpperCase())}
              placeholder="Ví dụ: SABC12DEF9"
              autoComplete="off"
              autoCapitalize="characters"
              // uppercase chi khi da co chu: neu khong placeholder cung bi viet hoa theo
              style={{ textTransform: pairingCode ? 'uppercase' : 'none', letterSpacing: 2, fontWeight: 600 }}
            />
          </label>

          <label className="sl-field">
            <span className="sl-label" style={{ display: 'flex', alignItems: 'center', gap: 'var(--sp-2)' }}>
              <Icon name="text" size={18} style={{ color: 'var(--rose-700)' }} />
              Tên hiển thị cho hộp
            </span>
            <input
              className="sl-input"
              value={boxName}
              onChange={(e) => setBoxName(e.target.value)}
              placeholder="Ví dụ: Hộp quà của Vợ Yêu"
            />
          </label>

          <Tips>Tên này chỉ hiện trên máy bạn — đổi lúc nào cũng được.</Tips>

          <Actions>
            <Button kind="pri" type="submit" disabled={isLoading}>
              {isLoading ? 'Đang ghép đôi…' : 'Ghép đôi ngay'}
            </Button>
          </Actions>
        </Body>
      </form>
    </Screen>
  );
}
