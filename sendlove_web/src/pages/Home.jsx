import React from 'react';
import { useNavigate } from 'react-router-dom';

export default function Home() {
  const navigate = useNavigate();

  return (
    <>
      <header className="nav-header">
        <div className="nav-logo">Sendlove</div>
        <button className="btn-secondary" style={{ width: 'auto', padding: '8px 16px' }} onClick={() => navigate('/')}>Logout</button>
      </header>
      
      <div className="page-content">
        <h1 className="title" style={{ fontSize: '2rem' }}>Welcome!</h1>
        <p className="subtitle">What would you like to do today?</p>

        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '20px', width: '100%', maxWidth: '600px' }}>
          
          <div className="glass-panel" style={{ width: '100%', cursor: 'pointer', textAlign: 'center' }} onClick={() => navigate('/send')}>
            <div style={{ fontSize: '3rem', marginBottom: '16px' }}>💝</div>
            <h3 style={{ margin: 0, color: 'var(--text-main)' }}>Send a Gift</h3>
            <p style={{ fontSize: '0.875rem', color: 'var(--text-muted)', marginTop: '8px' }}>Record and send video/audio messages to a box.</p>
          </div>

          <div className="glass-panel" style={{ width: '100%', cursor: 'pointer', textAlign: 'center' }} onClick={() => navigate('/manage')}>
            <div style={{ fontSize: '3rem', marginBottom: '16px' }}>⚙️</div>
            <h3 style={{ margin: 0, color: 'var(--text-main)' }}>Manage Box</h3>
            <p style={{ fontSize: '0.875rem', color: 'var(--text-muted)', marginTop: '8px' }}>Check status, battery, and configure your box.</p>
          </div>

        </div>
      </div>
    </>
  );
}
