import React, { useState } from 'react';
import { useNavigate } from 'react-router-dom';
import { unpairBox } from '../api/box';
import { useAuth } from '../context/AuthContext';
import Icon from './ui/Icon';
import { Button, CircleIcon, Modal } from './ui/Screen';

/**
 * Unpair confirmation popup, shared by sender and receiver. Unpairing removes
 * ONLY the caller: no messages, alarms or settings are deleted and the other
 * person is untouched — the copy below must say only that.
 */
const EFFECTS = {
  sender: [
    { icon: 'chat', text: 'Bạn không gửi được tin nhắn tới hộp này nữa.' },
    { icon: 'mailbox', text: 'Người nhận vẫn giữ hộp. Tin đã gửi vẫn còn trên hộp.' },
  ],
  receiver: [
    { icon: 'chat', text: 'Bạn không xem được tin nhắn và cài đặt của hộp nữa.' },
    { icon: 'bell', text: 'Báo thức đã đặt vẫn nằm trên hộp cho tới khi có người đổi.' },
  ],
};

export default function UnpairConfirm({ boxId, role, onClose, onError }) {
  const navigate = useNavigate();
  const { refreshProfile } = useAuth();
  const [busy, setBusy] = useState(false);

  const doUnpair = async () => {
    setBusy(true);
    try {
      await unpairBox(boxId);
      await refreshProfile();
      navigate('/dashboard', { replace: true });
    } catch (err) {
      setBusy(false);
      onClose();
      onError?.(err.response?.data?.error?.message || 'Không huỷ ghép đôi được.');
    }
  };

  return (
    <Modal onClose={busy ? undefined : onClose}>
      <span style={{ alignSelf: 'center' }}>
        <CircleIcon size={56} bg="var(--error-bg)" color="var(--error-fill)" icon="alert" iconSize={24} sw={2} />
      </span>
      <span className="sl-heading" style={{ textAlign: 'center' }}>Huỷ ghép đôi hộp này?</span>
      <span className="sl-body" style={{ textAlign: 'center' }}>
        Hộp sẽ rời khỏi tài khoản của bạn ngay lập tức.
      </span>

      <div style={{ display: 'flex', flexDirection: 'column', gap: 10, padding: '14px var(--sp-4)', borderRadius: 'var(--r-md)', background: 'var(--caramel-50)' }}>
        {(EFFECTS[role] || EFFECTS.receiver).map((e) => (
          <span key={e.text} style={{ display: 'flex', alignItems: 'center', gap: 'var(--sp-2)', color: 'var(--caramel-700)', fontSize: 12, lineHeight: 1.4 }}>
            <Icon name={e.icon} size={16} style={{ flex: '0 0 auto' }} /> {e.text}
          </span>
        ))}
      </div>

      {/* Re-pairing needs the code shown on the box (pairingCode /^[SR][A-Z0-9]{6,9}$/) */}
      <span className="sl-caption" style={{ textAlign: 'center', color: 'var(--neutral-400)' }}>
        Muốn dùng lại thì cần mã ghép đôi hiện trên màn hình hộp.
      </span>

      <Button kind="dan" onClick={doUnpair} disabled={busy}>
        {busy ? 'Đang huỷ…' : 'Huỷ ghép đôi'}
      </Button>
      <Button kind="gho" onClick={onClose} disabled={busy}>Giữ nguyên</Button>
    </Modal>
  );
}
