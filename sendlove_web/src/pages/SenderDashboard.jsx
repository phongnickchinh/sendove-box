import React from 'react';
import { useNavigate } from 'react-router-dom';

export default function SenderDashboard() {
  const navigate = useNavigate();

  return (
    <>
      <header className="nav-header">
        <div className="nav-logo">Sendlove</div>
        <button className="btn-secondary" style={{ width: 'auto', padding: '8px 16px' }} onClick={() => navigate('/')}>Logout</button>
      </header>
      
      <div className="page-content">
        <h1 className="title" style={{ fontSize: '2rem' }}>Send a Message</h1>
        <p className="subtitle">Record or select a video to send to the Box</p>

        <div className="glass-panel" style={{ maxWidth: '600px' }}>
          <div className="empty-state">
            <div className="empty-icon">🎥</div>
            <h3>No media selected</h3>
            <p>Upload a video or record a new one. The file will be automatically converted to be compatible with the Smart Box.</p>
          </div>

          <button className="btn-primary" style={{ marginBottom: '16px' }}>
            <span>📹</span> Record Video
          </button>
          <button className="btn-secondary">
            <span>📁</span> Choose from Device
          </button>
        </div>
      </div>
    </>
  );
}
