import React from 'react';

export default function SenderDashboard() {
  return (
    <div className="fade-in" style={{ display: 'flex', flexDirection: 'column', gap: '24px' }}>
      <div>
        <h1 style={{ fontSize: '2rem' }}>Send a Message</h1>
        <p style={{ color: 'var(--color-text-muted)' }}>Ghi âm hoặc chọn video để gửi tới Hộp quà</p>
      </div>

      <div className="glass-panel" style={{ padding: '32px', textAlign: 'center', display: 'flex', flexDirection: 'column', gap: '24px' }}>
        <div style={{ fontSize: '4rem' }}>🎥</div>
        <div>
          <h3 style={{ marginBottom: '8px' }}>Chưa có phương tiện nào</h3>
          <p style={{ color: 'var(--color-text-muted)' }}>
            Tải lên video (MP4/GIF) hoặc thu âm mới (Tối đa 15 giây).
            <br />Hệ thống sẽ tự động chuyển đổi định dạng cho Hộp quà.
          </p>
        </div>

        <div style={{ display: 'flex', gap: '16px', justifyContent: 'center', marginTop: '16px' }}>
          <button className="glass-button">
            <span>🎤</span> Thu âm ngay
          </button>
          <button className="glass-button" style={{ background: 'var(--color-white)', color: 'var(--color-text-main)', border: '1px solid var(--color-glass-border)' }}>
            <span>📁</span> Chọn Video từ máy
          </button>
        </div>
      </div>
    </div>
  );
}
