import { useState } from 'react';
import { useNavigate } from 'react-router-dom';
import apiClient from '../api/client';
import { useAuth } from '../context/AuthContext';
import Icon from '../components/ui/Icon';
import { Screen, Body, Button, Tips } from '../components/ui/Screen';

/**
 * The Figma "new-box" frame. A modal in the design; here a standalone /pair
 * route that only reproduces the dialog's card.
 */
/** Matches the backend's pairBoxSchema: S/R + 6–9 chars of A-Z0-9. */
const PAIRING_CODE = /^[SR][A-Z0-9]{6,9}$/;

export default function PairBox() {
  const [pairingCode, setPairingCode] = useState('');
  const [boxName, setBoxName] = useState('');
  const [error, setError] = useState('');
  const [isLoading, setIsLoading] = useState(false);

  const navigate = useNavigate();
  const { refreshProfile } = useAuth();

  const code = pairingCode.trim().toUpperCase();
  const roleHint = code.startsWith('S') ? 'Mã S — bạn sẽ là người gửi.'
    : code.startsWith('R') ? 'Mã R — bạn sẽ là người nhận.'
      : null;

  const handlePair = async (e) => {
    e.preventDefault();
    if (!code) { setError('Vui lòng nhập mã kết nối'); return; }
    // Validate here with the backend's exact pattern (validation.middleware.ts
    // pairBoxSchema) instead of sending a bad code and getting a 400.
    if (!PAIRING_CODE.test(code)) {
      setError(/^[SR]/.test(code)
        ? 'Mã phải gồm 7–10 ký tự, chỉ chữ in hoa và số.'
        : 'Mã bắt đầu bằng S (người gửi) hoặc R (người nhận).');
      return;
    }
    if (!boxName.trim()) { setError('Vui lòng đặt tên cho hộp quà của bạn'); return; }

    setIsLoading(true);
    setError('');
    try {
      const res = await apiClient.post('/boxes/pair', {
        pairingCode: code,
        boxName: boxName.trim(),
      });
      if (res.data.success) {
        // Reload the profile so the Dashboard lists the newly paired box.
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
      <Body center>
        <form onSubmit={handlePair} className="sl-pair-card" style={{ position: 'relative', textAlign: 'left' }}>
          {/* #9E6244 per Figma (carbon:close-outline) — same as the Login button
              text; it isn't in the token scale, hence the literal. */}
          <button
            type="button" className="sl-iconbtn" onClick={() => navigate('/dashboard')}
            aria-label="Đóng" style={{ position: 'absolute', top: -4, right: -4, color: '#9E6244' }}
          >
            <Icon name="x" size={24} />
          </button>

          <div>
            <div className="sl-label" style={{ textTransform: 'uppercase', letterSpacing: .4, fontSize: 17, fontWeight: 600, color: 'var(--neutral-700)' }}>
              Ghép hộp mới
            </div>
            <p className="sl-body" style={{ marginTop: 6 }}>
              Nhập mã hiện trên màn hình hộp. Mã bắt đầu bằng <b>S</b> nếu bạn là người gửi,
              hoặc <b>R</b> nếu bạn là người nhận.
            </p>
          </div>

          {error && <div className="sl-reason">{error}</div>}

          {/* fontWeight 600 per Figma (14/SemiBold) — .sl-label defaults to 500. */}
          <label className="sl-field">
            <span className="sl-label" style={{ display: 'flex', alignItems: 'center', gap: 'var(--sp-2)', fontWeight: 600 }}>
              <Icon name="key" size={12} style={{ color: 'var(--neutral-700)' }} />
              Mã kết nối
            </span>
            <input
              className="sl-input sl-input--accent"
              value={pairingCode}
              onChange={(e) => setPairingCode(e.target.value.toUpperCase())}
              placeholder="Ví dụ: SABC12DEF9"
              autoComplete="off"
              autoCapitalize="characters"
              style={{ textTransform: pairingCode ? 'uppercase' : 'none', letterSpacing: 2, fontWeight: 700 }}
            />
            <Tips>{roleHint || 'Mã gồm 7–10 ký tự, bắt đầu bằng S hoặc R.'}</Tips>
          </label>

          <label className="sl-field">
            <span className="sl-label" style={{ display: 'flex', alignItems: 'center', gap: 'var(--sp-2)', fontWeight: 600 }}>
              <Icon name="text" size={12} style={{ color: 'var(--neutral-700)' }} />
              Tên hiển thị cho hộp
            </span>
            <input
              className="sl-input sl-input--accent"
              value={boxName}
              onChange={(e) => setBoxName(e.target.value)}
              placeholder="Ví dụ: Hộp quà của Vợ Yêu 💕"
            />
          </label>

          {/* Full-width primary button like every other screen; exit with the corner X (no duplicate "Cancel"). */}
          <Button kind="pri" type="submit" disabled={isLoading} style={{ marginTop: 'var(--sp-2)' }}>
            {isLoading ? 'Đang ghép đôi…' : 'Ghép đôi ngay'}
          </Button>
        </form>
      </Body>
    </Screen>
  );
}
