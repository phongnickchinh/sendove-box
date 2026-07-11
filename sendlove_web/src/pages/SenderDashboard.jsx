import React, { useEffect, useState } from 'react';
import { useParams, useNavigate } from 'react-router-dom';
import { getMessages } from '../api/message';

export default function SenderDashboard() {
  const { boxId } = useParams();
  const navigate = useNavigate();
  const [messages, setMessages] = useState([]);
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    const fetchMessages = async () => {
      try {
        const res = await getMessages(boxId);
        if (res.success) {
          setMessages(res.data);
        }
      } catch (err) {
        console.error('Failed to fetch messages', err);
      } finally {
        setLoading(false);
      }
    };
    fetchMessages();
  }, [boxId]);

  return (
    <div className="fade-in" style={{ display: 'flex', flexDirection: 'column', gap: '24px', maxWidth: '600px', margin: '0 auto', padding: '20px' }}>
      <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
        <div>
          <h1 style={{ fontSize: '2rem' }}>Lịch sử tin nhắn</h1>
          <p style={{ color: 'var(--color-text-muted)' }}>Box: {boxId}</p>
        </div>
        <button className="glass-button primary" onClick={() => navigate(`/box/${boxId}/sender`)}>
          + Gửi tin mới
        </button>
      </div>

      {loading ? (
        <div style={{ textAlign: 'center', padding: '40px' }}>Đang tải...</div>
      ) : messages.length === 0 ? (
        <div className="glass-panel" style={{ padding: '32px', textAlign: 'center' }}>
          <div style={{ fontSize: '4rem', marginBottom: '16px' }}>📭</div>
          <h3>Chưa có tin nhắn nào</h3>
          <p style={{ color: 'var(--color-text-muted)', marginTop: '8px' }}>Hãy gửi yêu thương đến Hộp quà ngay bây giờ!</p>
        </div>
      ) : (
        <div style={{ display: 'flex', flexDirection: 'column', gap: '16px' }}>
          {messages.map(msg => (
            <div key={msg.id} className="glass-panel" style={{ display: 'flex', padding: '16px', gap: '16px', alignItems: 'center' }}>
              <div style={{ width: '60px', height: '60px', borderRadius: '8px', overflow: 'hidden', background: '#eee', flexShrink: 0 }}>
                {msg.thumbnail_url ? (
                  <img src={msg.thumbnail_url} alt="thumbnail" style={{ width: '100%', height: '100%', objectFit: 'cover' }} />
                ) : (
                  <div style={{ width: '100%', height: '100%', display: 'flex', alignItems: 'center', justifyContent: 'center', fontSize: '24px' }}>
                    {msg.type === 'voice' ? '🎤' : msg.type === 'text' ? '✍️' : '📁'}
                  </div>
                )}
              </div>
              <div style={{ flex: 1 }}>
                <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '4px' }}>
                  <span style={{ fontWeight: 'bold', textTransform: 'capitalize' }}>{msg.type}</span>
                  <span style={{ fontSize: '12px', color: 'var(--color-text-muted)' }}>
                    {new Date(msg.timestamp).toLocaleString('vi-VN')}
                  </span>
                </div>
                {msg.text && (
                  <p style={{ fontSize: '14px', color: 'var(--color-text-main)', display: '-webkit-box', WebkitLineClamp: 2, WebkitBoxOrient: 'vertical', overflow: 'hidden' }}>
                    "{msg.text}"
                  </p>
                )}
                {msg.duration && (
                  <p style={{ fontSize: '12px', color: 'var(--color-text-muted)', marginTop: '4px' }}>Thời lượng: {msg.duration}s</p>
                )}
              </div>
            </div>
          ))}
        </div>
      )}
    </div>
  );
}
