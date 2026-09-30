import React, { useCallback, useEffect, useRef, useState } from 'react';
import Icon from './Icon';

/**
 * Thông báo nổi ở mép trên màn hình — luôn nằm trong tầm mắt, kể cả khi nút vừa bấm ở
 * cuối một trang dài (thông báo đặt ở đầu trang từng nằm khuất 700px phía trên).
 * const [toast, showToast] = useToast(); showToast('Đã lưu'); showToast('Lỗi…', 'err');
 */
export function useToast() {
  const [toast, setToast] = useState(null);
  const timer = useRef(null);

  useEffect(() => () => clearTimeout(timer.current), []);

  const show = useCallback((text, kind = 'ok') => {
    clearTimeout(timer.current);
    setToast({ text, kind, id: Date.now() });
    timer.current = setTimeout(() => setToast(null), kind === 'err' ? 6000 : 3500);
  }, []);

  const node = toast && (
    <div key={toast.id} className={`sl-toast sl-toast--${toast.kind}`} role={toast.kind === 'err' ? 'alert' : 'status'}>
      <Icon name={toast.kind === 'err' ? 'alert' : 'check'} size={16} />
      <span>{toast.text}</span>
    </div>
  );

  return [node, show];
}
