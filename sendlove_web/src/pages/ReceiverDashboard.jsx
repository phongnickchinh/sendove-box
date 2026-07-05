import React from 'react';
import { useNavigate } from 'react-router-dom';

export default function ReceiverDashboard() {
  const navigate = useNavigate();

  return (
    <>
      <header className="nav-header">
        <div className="nav-logo">Box Management</div>
        <button className="btn-secondary" style={{ width: 'auto', padding: '8px 16px' }} onClick={() => navigate('/')}>Logout</button>
      </header>
      
      <div className="page-content">
        <h1 className="title" style={{ fontSize: '2rem' }}>Your Sendlove Box</h1>
        <p className="subtitle">Status and settings</p>

        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '20px', width: '100%', maxWidth: '600px' }}>
          
          <div className="glass-panel" style={{ width: '100%' }}>
            <h3 style={{ marginTop: 0, color: 'var(--text-muted)' }}>Battery Level</h3>
            <div style={{ fontSize: '2.5rem', fontWeight: 'bold', color: 'var(--accent)' }}>
              --%
            </div>
            <p style={{ fontSize: '0.875rem', color: 'var(--text-muted)' }}>Last synced: Unknown</p>
          </div>

          <div className="glass-panel" style={{ width: '100%' }}>
            <h3 style={{ marginTop: 0, color: 'var(--text-muted)' }}>Current Alarm</h3>
            <div style={{ fontSize: '2.5rem', fontWeight: 'bold', color: 'var(--text-main)' }}>
              --:--
            </div>
            <button className="btn-secondary" style={{ marginTop: '16px', padding: '8px' }}>Change</button>
          </div>

        </div>

        <div className="glass-panel" style={{ width: '100%', maxWidth: '600px', marginTop: '20px' }}>
          <h3 style={{ marginTop: 0 }}>Message History</h3>
          <div className="empty-state" style={{ padding: '40px 20px', margin: '16px 0 0' }}>
            <div className="empty-icon">📫</div>
            <h3>No messages yet</h3>
            <p>When someone sends you a message, it will appear here.</p>
          </div>
        </div>

      </div>
    </>
  );
}
