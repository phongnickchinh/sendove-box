import { useState } from 'react';
import { useNavigate } from 'react-router-dom';
import { useAuth } from '../context/AuthContext';
import { Package, LogOut, Plus } from 'lucide-react';
import { logOut } from '../api/auth';

export default function Dashboard() {
  const { user, profile } = useAuth();
  const navigate = useNavigate();

  const handleLogout = async () => {
    await logOut();
    navigate('/');
  };

  return (
    <div className="fade-in">
      <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '32px' }}>
        <div>
          <h1 className="text-gradient" style={{ fontSize: '2.5rem' }}>Xin chào, {user?.displayName?.split(' ')[0] || 'bạn'}!</h1>
          <p style={{ color: 'var(--color-text-muted)' }}>Hãy chọn một hộp quà để tiếp tục.</p>
        </div>
        <button onClick={handleLogout} className="glass-button" style={{ padding: '8px 16px', fontSize: '0.9rem' }}>
          <LogOut size={18} strokeWidth={1.5} /> Đăng xuất
        </button>
      </div>

      <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fill, minmax(300px, 1fr))', gap: '24px' }}>
        {/* Placeholder for Box List */}
        {profile?.boxes_list && Object.keys(profile.boxes_list).length > 0 ? (
          Object.entries(profile.boxes_list).map(([boxId, box]) => (
            <div 
              key={boxId} 
              className="glass-panel slide-up" 
              style={{ padding: '24px', cursor: 'pointer', transition: 'var(--transition-smooth)' }}
              onClick={() => navigate(`/box/${boxId}/${box.role === 'sender' ? 'sender' : 'receiver'}`)}
            >
              <div style={{ display: 'flex', alignItems: 'center', gap: '16px' }}>
                <div style={{ padding: '12px', background: 'var(--color-primary-light)', borderRadius: '16px', color: 'white' }}>
                  <Package size={32} strokeWidth={1.5} />
                </div>
                <div>
                  <h3 style={{ fontSize: '1.2rem', marginBottom: '4px' }}>{box.box_name}</h3>
                  <span style={{ 
                    fontSize: '0.8rem', 
                    padding: '4px 12px', 
                    borderRadius: '12px', 
                    background: 'rgba(255,255,255,0.8)',
                    color: box.role === 'sender' ? 'var(--color-primary)' : 'var(--color-secondary)',
                    fontWeight: 600
                  }}>
                    {box.role === 'sender' ? 'Người Gửi' : 'Người Nhận'}
                  </span>
                </div>
              </div>
            </div>
          ))
        ) : (
          <div className="glass-panel" style={{ padding: '40px', textAlign: 'center', gridColumn: '1 / -1' }}>
            <p style={{ color: 'var(--color-text-muted)', marginBottom: '16px' }}>Bạn chưa có hộp quà nào.</p>
          </div>
        )}

        {/* Add New Box Card */}
        <div 
          className="glass-panel slide-up" 
          style={{ 
            padding: '24px', 
            cursor: 'pointer', 
            display: 'flex', 
            flexDirection: 'column', 
            alignItems: 'center', 
            justifyContent: 'center',
            border: '2px dashed var(--color-primary-light)',
            background: 'rgba(255, 255, 255, 0.3)',
            minHeight: '120px'
          }}
          onClick={() => navigate('/pair')}
        >
          <div style={{ color: 'var(--color-primary)', marginBottom: '8px' }}>
            <Plus size={32} strokeWidth={1.5} />
          </div>
          <p style={{ fontWeight: 600, color: 'var(--color-primary)' }}>Kết nối Box mới</p>
        </div>
      </div>
    </div>
  );
}
