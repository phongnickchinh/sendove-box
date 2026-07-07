import React from 'react';

export default function ReceiverDashboard() {
  return (
    <div className="fade-in" style={{ display: 'flex', flexDirection: 'column', gap: '24px' }}>
      <div>
        <h1 style={{ fontSize: '2rem' }}>Quản lý Hộp quà</h1>
        <p style={{ color: 'var(--color-text-muted)' }}>Cài đặt báo thức và xem trạng thái hộp của bạn.</p>
      </div>

      <div className="glass-panel" style={{ padding: '32px', display: 'flex', flexDirection: 'column', gap: '24px' }}>
        <h3 className="text-gradient">Trạng thái hiện tại</h3>
        
        <div style={{ display: 'flex', gap: '24px', flexWrap: 'wrap' }}>
          {/* Box Status Card */}
          <div style={{ 
            flex: '1', minWidth: '250px', 
            background: 'rgba(255, 255, 255, 0.4)', 
            padding: '24px', borderRadius: '16px',
            display: 'flex', flexDirection: 'column', gap: '12px'
          }}>
            <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
              <span style={{ fontWeight: 600 }}>Tình trạng</span>
              <span style={{ padding: '4px 12px', background: '#34c759', color: 'white', borderRadius: '12px', fontSize: '0.8rem', fontWeight: 600 }}>Online</span>
            </div>
            <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
              <span style={{ fontWeight: 600 }}>Pin</span>
              <span>🔋 85%</span>
            </div>
            <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
              <span style={{ fontWeight: 600 }}>Cập nhật lần cuối</span>
              <span style={{ color: 'var(--color-text-muted)', fontSize: '0.9rem' }}>Vài giây trước</span>
            </div>
          </div>

          {/* Alarm Config Card */}
          <div style={{ 
            flex: '1', minWidth: '250px', 
            background: 'rgba(255, 255, 255, 0.4)', 
            padding: '24px', borderRadius: '16px',
            display: 'flex', flexDirection: 'column', gap: '12px'
          }}>
            <h4 style={{ color: 'var(--color-text-main)' }}>Báo thức & Nhắc nhở</h4>
            <div style={{ display: 'flex', gap: '16px', alignItems: 'center' }}>
              <input type="time" className="glass-input" defaultValue="07:30" style={{ flex: 1 }} />
              <button className="glass-button" style={{ padding: '14px 24px' }}>Lưu</button>
            </div>
          </div>
        </div>
      </div>
    </div>
  );
}
