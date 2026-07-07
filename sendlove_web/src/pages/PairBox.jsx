import { useState } from 'react';
import { useNavigate } from 'react-router-dom';
import { ArrowLeft, KeySquare, Tag, Loader2 } from 'lucide-react';
import apiClient from '../api/client';
import { useAuth } from '../context/AuthContext';

export default function PairBox() {
  const [pairingCode, setPairingCode] = useState('');
  const [boxName, setBoxName] = useState('');
  const [error, setError] = useState('');
  const [isLoading, setIsLoading] = useState(false);
  
  const navigate = useNavigate();
  const { refreshProfile } = useAuth();

  const handlePair = async (e) => {
    e.preventDefault();
    if (!pairingCode.trim()) {
      setError('Vui lòng nhập mã kết nối');
      return;
    }
    if (!boxName.trim()) {
      setError('Vui lòng đặt tên cho hộp quà của bạn');
      return;
    }
    
    setIsLoading(true);
    setError('');
    
    try {
      const res = await apiClient.post('/boxes/pair', {
        pairingCode: pairingCode.trim().toUpperCase(),
        boxName: boxName.trim()
      });
      
      if (res.data.success) {
        // Tải lại thông tin user để cập nhật danh sách Box
        await refreshProfile();
        navigate('/dashboard');
      }
    } catch (err) {
      console.error(err);
      setError(err.response?.data?.error?.message || err.response?.data?.message || 'Có lỗi xảy ra khi kết nối Box. Vui lòng thử lại.');
    } finally {
      setIsLoading(false);
    }
  };

  return (
    <div className="fade-in" style={{ display: 'flex', flexDirection: 'column', alignItems: 'center', paddingTop: '40px' }}>
      <div style={{ width: '100%', maxWidth: '480px' }}>
        <button 
          onClick={() => navigate('/dashboard')}
          style={{
            background: 'none', border: 'none', color: 'var(--color-primary)',
            display: 'flex', alignItems: 'center', gap: '8px', cursor: 'pointer',
            fontSize: '1rem', fontWeight: 600, marginBottom: '24px'
          }}
        >
          <ArrowLeft size={20} strokeWidth={1.5} /> Quay lại
        </button>

        <div className="glass-panel slide-up" style={{ padding: '32px' }}>
          <h2 className="text-gradient" style={{ fontSize: '2rem', marginBottom: '8px', textAlign: 'center' }}>Kết nối Box mới</h2>
          <p style={{ color: 'var(--color-text-muted)', textAlign: 'center', marginBottom: '24px', fontSize: '0.9rem' }}>
            Nhập mã kết nối trên màn hình Box để ghép đôi. Mã bắt đầu bằng <b style={{color: 'var(--color-primary)'}}>S</b> (Người gửi) hoặc <b style={{color: 'var(--color-secondary)'}}>R</b> (Người nhận).
          </p>

          {error && (
            <div style={{ padding: '12px', background: 'rgba(255, 59, 48, 0.1)', color: '#ff3b30', borderRadius: '12px', marginBottom: '20px', fontSize: '0.9rem', textAlign: 'center' }}>
              {error}
            </div>
          )}

          <form onSubmit={handlePair} style={{ display: 'flex', flexDirection: 'column', gap: '20px' }}>
            <div>
              <label style={{ display: 'flex', alignItems: 'center', gap: '8px', marginBottom: '8px', fontWeight: 500, color: 'var(--color-text-main)' }}>
                <KeySquare size={18} strokeWidth={1.5} color="var(--color-primary)" />
                Mã kết nối (Pairing Code)
              </label>
              <input 
                type="text" 
                className="glass-input" 
                placeholder="Ví dụ: S1A2B3C" 
                value={pairingCode}
                onChange={(e) => setPairingCode(e.target.value.toUpperCase())}
                style={{ textTransform: 'uppercase', letterSpacing: '2px', fontWeight: 600 }}
              />
            </div>

            <div>
              <label style={{ display: 'flex', alignItems: 'center', gap: '8px', marginBottom: '8px', fontWeight: 500, color: 'var(--color-text-main)' }}>
                <Tag size={18} strokeWidth={1.5} color="var(--color-primary)" />
                Tên hiển thị cho Box
              </label>
              <input 
                type="text" 
                className="glass-input" 
                placeholder="Ví dụ: Hộp quà của Vợ Yêu" 
                value={boxName}
                onChange={(e) => setBoxName(e.target.value)}
              />
            </div>

            <button 
              type="submit" 
              className="glass-button" 
              disabled={isLoading}
              style={{ marginTop: '12px', width: '100%', opacity: isLoading ? 0.7 : 1 }}
            >
              {isLoading ? <Loader2 size={20} strokeWidth={1.5} className="spin" /> : 'Ghép Đôi Ngay'}
            </button>
          </form>
        </div>
      </div>
    </div>
  );
}
