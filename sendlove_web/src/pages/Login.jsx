import { useState, useEffect } from 'react';
import { useNavigate } from 'react-router-dom';
import { signInWithGoogle, signInWithFacebook } from '../api/auth';
import { useAuth } from '../context/AuthContext';
import { Screen } from '../components/ui/Screen';

/**
 * Frame "login signup" trong Figma. Ảnh nền thật (anthony-melone…unsplash)
 * chưa có — thử dùng src/assets/hero.png có sẵn trong repo nhưng nó là một
 * hoạ tiết trang trí NỀN TRONG SUỐT (dải ruy băng), không phải ảnh chụp toàn
 * khung, nên object-fit:cover làm nó méo và hở nền kem phía sau. Dùng tạm
 * gradient ấm thay vì ảnh vỡ; khi có ảnh thật, thay khối .sl-login-hero
 * bằng <img> trỏ file mới.
 * Bố cục dùng flex căn giữa dọc thay vì toạ độ tuyệt đối của thiết kế
 * (logo y=271, card y=344…) để không vỡ trên các chiều cao màn hình khác
 * iPhone 14 Pro Max — một đơn giản hoá có chủ đích.
 */

/** Logo Google — giữ nguyên 4 màu gốc, brand guideline không cho tô lại. */
function GoogleMark() {
  return (
    <svg width="18" height="18" viewBox="0 0 24 24" aria-hidden="true">
      <path d="M22.56 12.25c0-.78-.07-1.53-.2-2.25H12v4.26h5.92c-.26 1.37-1.04 2.53-2.21 3.31v2.77h3.57c2.08-1.92 3.28-4.74 3.28-8.09z" fill="#4285F4" />
      <path d="M12 23c2.97 0 5.46-.98 7.28-2.66l-3.57-2.77c-.98.66-2.23 1.06-3.71 1.06-2.86 0-5.29-1.93-6.16-4.53H2.18v2.84C3.99 20.53 7.7 23 12 23z" fill="#34A853" />
      <path d="M5.84 14.09c-.22-.66-.35-1.36-.35-2.09s.13-1.43.35-2.09V7.07H2.18C1.43 8.55 1 10.22 1 12s.43 3.45 1.18 4.93l2.85-2.22.81-.62z" fill="#FBBC05" />
      <path d="M12 5.38c1.62 0 3.06.56 4.21 1.64l3.15-3.15C17.45 2.09 14.97 1 12 1 7.7 1 3.99 3.47 2.18 7.07l3.66 2.84c.87-2.6 3.3-4.53 6.16-4.53z" fill="#EA4335" />
    </svg>
  );
}

/** Logo Facebook — giữ nguyên brand guideline (chữ f trắng trên nền xanh #1877F2). */
function FacebookMark() {
  return (
    <svg width="18" height="18" viewBox="0 0 24 24" aria-hidden="true">
      <circle cx="12" cy="12" r="12" fill="#1877F2" />
      <path d="M15.5 8h-1.8c-.4 0-.7.3-.7.8V10h2.4l-.3 2.2h-2.1V19h-2.3v-6.8H8.6V10h1.9V8.6C10.5 6.7 11.5 5.5 13.4 5.5h2.1V8z" fill="#fff" />
    </svg>
  );
}

export default function Login() {
  const [loading, setLoading] = useState(null); // null | 'google' | 'facebook'
  const [error, setError] = useState(null);
  const navigate = useNavigate();
  const { user } = useAuth();

  useEffect(() => {
    if (user) navigate('/dashboard', { replace: true });
  }, [user, navigate]);

  const runLogin = async (provider, fn) => {
    setLoading(provider);
    setError(null);
    const res = await fn();
    if (res.error) {
      setError(
        res.error.code === 'auth/operation-not-allowed'
          ? 'Cách đăng nhập này chưa được bật cho ứng dụng. Thử Google, hoặc báo cho quản trị viên.'
          : 'Đăng nhập thất bại. Vui lòng thử lại.'
      );
      setLoading(null);
      return;
    }
    // Không tự điều hướng: useEffect ở trên chạy khi AuthContext có user.
  };

  return (
    <Screen>
      <div className="sl-login-hero" aria-hidden="true" />
      <div className="sl-login-content">
        <span className="sl-login-logo">SendloveBox</span>

        <div className="sl-login-card">
          <div>
            <div className="sl-login-headline">Nhớ ai đó?</div>
            <div className="sl-login-sub">Đăng nhập và gửi chút yêu thương.</div>
          </div>

          {error && <div className="sl-reason">{error}</div>}

          <div style={{ display: 'flex', flexDirection: 'column', gap: 12 }}>
            <button
              type="button" className="sl-login-btn"
              onClick={() => runLogin('google', signInWithGoogle)}
              disabled={loading !== null}
            >
              <GoogleMark />
              {loading === 'google' ? 'Đang kết nối…' : 'Đăng nhập bằng Google'}
            </button>
            <button
              type="button" className="sl-login-btn"
              onClick={() => runLogin('facebook', signInWithFacebook)}
              disabled={loading !== null}
            >
              <FacebookMark />
              {loading === 'facebook' ? 'Đang kết nối…' : 'Đăng nhập bằng Facebook'}
            </button>
          </div>
        </div>

        <p className="sl-login-terms">
          Khi tiếp tục, bạn đồng ý với Điều khoản dịch vụ &amp; Chính sách quyền riêng tư của chúng tôi.
        </p>
      </div>
    </Screen>
  );
}
