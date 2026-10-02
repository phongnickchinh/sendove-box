import { useState, useSyncExternalStore } from 'react';
import Icon from './ui/Icon';
import {
  currentInstallMode, isHintDismissed, dismissHint,
  subscribeInstallPrompt, getInstallPrompt, promptInstall,
} from '../utils/installHint';

/** Warning for the in-app browsers of Zalo / Facebook / Messenger, where Google refuses sign-in. */
export function InAppBrowserWarning() {
  const [mode] = useState(currentInstallMode);
  if (mode !== 'in-app') return null;
  return (
    <div className="sl-install sl-install--warn">
      <Icon name="alert" size={16} />
      <span className="sl-install__text">
        Trình duyệt trong Zalo, Facebook hay Messenger thường chặn đăng nhập Google.
        Hãy mở trang này bằng Safari hoặc Chrome.
      </span>
    </div>
  );
}

/**
 * "Add to home screen" hint: points at Share on iPhone (no install prompt),
 * opens the install dialog elsewhere. Hidden once installed or dismissed.
 * card = standalone card for the Dashboard (signed-in users never see Login).
 */
export default function InstallHint({ card = false }) {
  const [mode] = useState(currentInstallMode);
  const [dismissed, setDismissed] = useState(isHintDismissed);
  const installPrompt = useSyncExternalStore(subscribeInstallPrompt, getInstallPrompt);

  const showIOS = mode === 'ios';
  const showPrompt = mode === 'other' && installPrompt !== null;
  if (dismissed || (!showIOS && !showPrompt)) return null;

  const hide = () => { dismissHint(); setDismissed(true); };

  return (
    <div className={`sl-install${card ? ' sl-install--card' : ''}`}>
      <Icon name="phone" size={16} />
      {showIOS ? (
        <span className="sl-install__text">
          Mở nhanh như một ứng dụng: bấm <Icon name="share" size={16} className="sl-install__glyph" role="img" aria-label="nút Chia sẻ" aria-hidden="false" /> rồi
          chọn “Thêm vào Màn hình chính”.
        </span>
      ) : (
        <span className="sl-install__text">
          Mở nhanh như một ứng dụng.{' '}
          <button type="button" className="sl-install__action" onClick={promptInstall}>
            Cài lên màn hình chính
          </button>
        </span>
      )}
      <button type="button" className="sl-install__close" onClick={hide} aria-label="Ẩn gợi ý">
        <Icon name="x" size={16} />
      </button>
    </div>
  );
}
