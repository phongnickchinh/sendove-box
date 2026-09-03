import { useState, useEffect } from 'react';
import { useNavigate } from 'react-router-dom';
import { signInWithGoogle } from '../api/auth';
import { useAuth } from '../context/AuthContext';
import { Screen, Body, Actions, Button, CircleIcon } from '../components/ui/Screen';

/** Logo Google — giữ nguyên 4 màu gốc, brand guideline không cho tô lại. */
function GoogleMark() {
  return (
    <svg width="20" height="20" viewBox="0 0 24 24" aria-hidden="true">
      <path d="M22.56 12.25c0-.78-.07-1.53-.2-2.25H12v4.26h5.92c-.26 1.37-1.04 2.53-2.21 3.31v2.77h3.57c2.08-1.92 3.28-4.74 3.28-8.09z" fill="#4285F4" />
      <path d="M12 23c2.97 0 5.46-.98 7.28-2.66l-3.57-2.77c-.98.66-2.23 1.06-3.71 1.06-2.86 0-5.29-1.93-6.16-4.53H2.18v2.84C3.99 20.53 7.7 23 12 23z" fill="#34A853" />
      <path d="M5.84 14.09c-.22-.66-.35-1.36-.35-2.09s.13-1.43.35-2.09V7.07H2.18C1.43 8.55 1 10.22 1 12s.43 3.45 1.18 4.93l2.85-2.22.81-.62z" fill="#FBBC05" />
      <path d="M12 5.38c1.62 0 3.06.56 4.21 1.64l3.15-3.15C17.45 2.09 14.97 1 12 1 7.7 1 3.99 3.47 2.18 7.07l3.66 2.84c.87-2.6 3.3-4.53 6.16-4.53z" fill="#EA4335" />
    </svg>
  );
}

export default function Login() {
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState(null);
  const navigate = useNavigate();
  const { user } = useAuth();

  useEffect(() => {
    if (user) navigate('/dashboard', { replace: true });
  }, [user, navigate]);

  const handleGoogleLogin = async () => {
    setIsLoading(true);
    setError(null);
    const res = await signInWithGoogle();
    if (res.error) {
      setError('Đăng nhập thất bại. Vui lòng thử lại.');
      setIsLoading(false);
      return;
    }
    // Không tự điều hướng: useEffect ở trên chạy khi AuthContext có user.
  };

  return (
    <Screen>
      <Body center>
        <CircleIcon size={72} bg="var(--rose-50)" color="var(--rose-400)" icon="chat" iconSize={32} sw={2} />
        <h1 className="sl-title" style={{ marginTop: 'var(--sp-5)' }}>Sendlove Box</h1>
        <p className="sl-body" style={{ marginTop: 'var(--sp-2)' }}>
          Gửi một lời nhắn, và để chiếc hộp mang nó tới tận nơi.
        </p>

        {error && <div className="sl-reason" style={{ marginTop: 'var(--sp-5)' }}>{error}</div>}

        {/* Actions nằm TRONG Body: .sl-acts dùng margin-top:auto và thừa hưởng
            padding ngang 20 của thân, đặt ra ngoài là mất lề hai bên. */}
        <Actions>
          <Button kind="pri" onClick={handleGoogleLogin} disabled={isLoading}>
            {isLoading ? 'Đang kết nối…' : <><GoogleMark /> Tiếp tục với Google</>}
          </Button>
          <span className="sl-caption" style={{ textAlign: 'center', color: 'var(--neutral-400)' }}>
            © 2026 Sendlove Box Project
          </span>
        </Actions>
      </Body>
    </Screen>
  );
}
