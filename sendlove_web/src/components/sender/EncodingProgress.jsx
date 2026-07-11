import React from 'react';

const EncodingProgress = ({ phase, progress, onCancel }) => {
  // phase: 'encoding' | 'uploading' | 'done' | 'error'
  
  const getPhaseText = () => {
    switch (phase) {
      case 'encoding': return 'Đang xử lý hình ảnh & âm thanh...';
      case 'uploading': return 'Đang gửi lên đám mây...';
      case 'done': return 'Đã gửi thành công! 🎉';
      case 'error': return 'Có lỗi xảy ra 😢';
      default: return 'Vui lòng chờ...';
    }
  };

  return (
    <div className="encoding-progress glass-panel fade-in" style={{ padding: '30px', textAlign: 'center' }}>
      <h3 style={{ marginBottom: '20px' }}>{getPhaseText()}</h3>
      
      {phase !== 'done' && phase !== 'error' && (
        <div style={{ width: '100%', height: '10px', background: '#eee', borderRadius: '5px', overflow: 'hidden', marginBottom: '10px' }}>
          <div style={{ 
            height: '100%', 
            width: `${progress}%`, 
            background: 'linear-gradient(90deg, var(--color-primary-light), var(--color-primary))',
            transition: 'width 0.3s ease'
          }} />
        </div>
      )}
      
      {phase !== 'done' && phase !== 'error' && (
        <p style={{ fontSize: '14px', fontWeight: 'bold' }}>{progress}%</p>
      )}

      {phase === 'done' && (
        <div style={{ margin: '20px 0' }}>
          <button className="glass-button" onClick={onCancel}>Quay về trang chủ</button>
        </div>
      )}

      {phase === 'error' && (
        <div style={{ margin: '20px 0' }}>
          <button className="glass-button" onClick={onCancel}>Thử lại</button>
        </div>
      )}
    </div>
  );
};

export default EncodingProgress;
