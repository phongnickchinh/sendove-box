import React, { useCallback, useEffect, useRef, useState } from 'react';
import Icon from './Icon';

/**
 * Floating notice at the top of the viewport — always in sight, even when the
 * button just pressed is at the bottom of a long page.
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
