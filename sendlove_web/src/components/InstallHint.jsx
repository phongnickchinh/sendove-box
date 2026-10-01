import { useState, useSyncExternalStore } from 'react';
import Icon from './ui/Icon';
import {
  currentInstallMode, isHintDismissed, dismissHint,
  subscribeInstallPrompt, getInstallPrompt, promptInstall,
} from '../utils/installHint';

/**
 * Warning shown when the page opens in the in-app browser of Zalo / Facebook /
 * Messenger: Google refuses sign-in there and the user only sees a cryptic
 * error. Box links are usually shared through these apps, so this is a common
 * entry point.
 */
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
 * "Add to home screen" hint. iPhone has no automatic install prompt, so it
 * points at the Share button; on Android/desktop a tap opens the browser's
 * install dialog. Hidden once installed or dismissed.
 *
 * card: render as a standalone card (Dashboard). The default is the last line
 * of the login card. It must also appear on the Dashboard because signed-in
 * users never see the Login screen.
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
