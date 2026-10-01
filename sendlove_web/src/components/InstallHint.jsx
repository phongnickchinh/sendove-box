import { useState, useSyncExternalStore } from 'react';
import Icon from './ui/Icon';
import {
  currentInstallMode, isHintDismissed, dismissHint,
  subscribeInstallPrompt, getInstallPrompt, promptInstall,
} from '../utils/installHint';

/**
 * Cảnh báo khi trang mở trong trình duyệt nhúng của Zalo/Facebook/Messenger:
 * Google từ chối đăng nhập ở đó, người dùng chỉ thấy lỗi khó hiểu. Link hộp quà
 * thường được gửi qua chính các app này nên đây là đường vào phổ biến.
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
 * Gợi ý đưa app lên màn hình chính. iPhone không có lời mời cài tự động nên
 * phải chỉ đường qua nút Chia sẻ; Android/desktop thì bấm là mở hộp thoại cài
 * của trình duyệt. Đã cài rồi, hoặc đã bấm ẩn, thì không hiện.
 *
 * card: dựng thành thẻ riêng (Dashboard). Mặc định là dòng cuối của thẻ đăng nhập.
 * Phải có mặt ở Dashboard vì người đã đăng nhập không bao giờ thấy màn Login.
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
