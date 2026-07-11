import React, { useEffect, useState } from 'react';
import { useParams } from 'react-router-dom';
import { getMessages } from '../api/message';
import apiClient from '../api/client';

export default function ReceiverUI() {
  const { boxId } = useParams();
  const [messages, setMessages] = useState([]);
  const [boxStatus, setBoxStatus] = useState(null);
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    const fetchData = async () => {
      try {
        // Fetch message history
        const msgRes = await getMessages(boxId);
        if (msgRes.success) {
          setMessages(msgRes.data);
        }
        
        // Fetch box status (if you have an API for this, for now just an assumption)
        const boxRes = await apiClient.get(`/boxes/${boxId}`);
        if (boxRes.data.success) {
          setBoxStatus(boxRes.data.data.status);
        }
      } catch (err) {
        console.error('Failed to fetch receiver data', err);
      } finally {
        setLoading(false);
      }
    };
    fetchData();
  }, [boxId]);

  return (
    <div className="fade-in" style={{ display: 'flex', flexDirection: 'column', gap: '24px', maxWidth: '600px', margin: '0 auto', padding: '20px' }}>
      <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
        <div>
          <h1 style={{ fontSize: '2rem' }}>Hộp quà của tôi</h1>
          <p style={{ color: 'var(--color-text-muted)' }}>Mã Box: {boxId}</p>
        </div>
      </div>

      <div className="glass-panel" style={{ padding: '20px', display: 'flex', gap: '20px', alignItems: 'center' }}>
        <div style={{ fontSize: '3rem' }}>🎁</div>
        <div style={{ flex: 1 }}>
          <h3 style={{ marginBottom: '8px' }}>Trạng thái Box</h3>
          {boxStatus ? (
            <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '10px', fontSize: '14px' }}>
              <div>Trạng thái: <strong style={{ color: boxStatus.online ? 'green' : 'red' }}>{boxStatus.online ? 'Online' : 'Offline'}</strong></div>
              <div>Pin: <strong>{boxStatus.battery}% {boxStatus.charging ? '⚡' : ''}</strong></div>
              <div style={{ gridColumn: '1 / -1' }}>
                Cập nhật lần cuối: {new Date(boxStatus.last_seen).toLocaleString('vi-VN')}
              </div>
            </div>
          ) : (
            <p style={{ color: 'var(--color-text-muted)' }}>Đang tải trạng thái...</p>
          )}
        </div>
      </div>

      <div>
        <h2 style={{ fontSize: '1.5rem', marginBottom: '16px' }}>Tin nhắn nhận được</h2>
        
        {loading ? (
          <div style={{ textAlign: 'center', padding: '40px' }}>Đang tải...</div>
        ) : messages.length === 0 ? (
          <div className="glass-panel" style={{ padding: '32px', textAlign: 'center' }}>
            <div style={{ fontSize: '3rem', marginBottom: '16px' }}>📭</div>
            <h3>Chưa có tin nhắn nào</h3>
            <p style={{ color: 'var(--color-text-muted)', marginTop: '8px' }}>Người ấy chưa gửi tin nhắn nào cho bạn.</p>
          </div>
        ) : (
          <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fill, minmax(150px, 1fr))', gap: '16px' }}>
            {messages.map(msg => (
              <div key={msg.id} className="glass-panel" style={{ padding: '10px', display: 'flex', flexDirection: 'column', alignItems: 'center', textAlign: 'center' }}>
                <div style={{ width: '100px', height: '100px', borderRadius: '8px', overflow: 'hidden', background: '#eee', marginBottom: '10px' }}>
                  {msg.thumbnail_url ? (
                    <img src={msg.thumbnail_url} alt="thumbnail" style={{ width: '100%', height: '100%', objectFit: 'cover' }} />
                  ) : (
                    <div style={{ width: '100%', height: '100%', display: 'flex', alignItems: 'center', justifyContent: 'center', fontSize: '32px' }}>
                      {msg.type === 'voice' ? '🎤' : msg.type === 'text' ? '✍️' : '📁'}
                    </div>
                  )}
                </div>
                <div style={{ fontSize: '12px', color: 'var(--color-text-muted)', marginBottom: '4px' }}>
                  {new Date(msg.timestamp).toLocaleDateString('vi-VN')}
                </div>
                <div style={{ fontWeight: 'bold', fontSize: '14px', textTransform: 'capitalize' }}>
                  {msg.type}
                </div>
              </div>
            ))}
          </div>
        )}
      </div>
    </div>
  );
}
