import React, { useState } from 'react';
import { useNavigate } from 'react-router-dom';
import { auth, googleProvider } from '../config/firebase';
import { signInWithPopup } from 'firebase/auth';

export default function Login() {
  const navigate = useNavigate();
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState('');

  const handleGoogleLogin = async () => {
    setLoading(true);
    setError('');

    // Bỏ qua gọi API thật nếu Firebase đang dùng Key giữ chỗ (Placeholder)
    if (auth.app.options.apiKey === "YOUR_API_KEY") {
      setTimeout(() => {
        setLoading(false);
        navigate('/home');
      }, 1000);
      return;
    }

    try {
      // Gọi hàm đăng nhập bằng popup của Firebase
      await signInWithPopup(auth, googleProvider);
      navigate('/home');
    } catch (err) {
      console.error(err);
      setError(err.message);
      // NOTE: Chuyển hướng tạm thời
      setTimeout(() => {
        navigate('/home');
      }, 1500);
    } finally {
      setLoading(false);
    }
  };

  return (
    <div className="glass-panel" style={{ marginTop: '10vh' }}>
      <div className="title">Sendlove Box</div>
      <div className="subtitle">Sign in to continue</div>
      
      {error && (
        <div style={{ color: '#f87171', fontSize: '0.875rem', marginBottom: '16px', textAlign: 'center', background: 'rgba(239, 68, 68, 0.1)', padding: '8px', borderRadius: '4px' }}>
          {error} <br/>(Mocking login in 1.5s...)
        </div>
      )}

      <div style={{ display: 'flex', flexDirection: 'column', gap: '10px' }}>
        <button onClick={handleGoogleLogin} className="btn-primary" disabled={loading} style={{ background: 'white', color: '#333' }}>
          <img src="https://www.gstatic.com/firebasejs/ui/2.0.0/images/auth/google.svg" alt="Google" style={{ width: '24px', height: '24px' }} />
          {loading ? 'Authenticating...' : 'Sign in with Google'}
        </button>
      </div>
    </div>
  );
}
