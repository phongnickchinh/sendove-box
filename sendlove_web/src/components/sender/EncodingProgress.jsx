import React from 'react';
import Icon from '../ui/Icon';
import { Screen, AppBar, Body, Actions, Button, Chip, CircleIcon, Modal } from '../ui/Screen';

/**
 * Ba màn của bước 3 trong thiết kế:
 *   03-convert-popup   -> phase 'encoding' | 'uploading'  (popup đè lên nền bước 2)
 *   04-send-successful -> phase 'done'
 *   05-send-fail       -> phase 'error'
 * Popup là chỗ duy nhất trong luồng được phép có đổ bóng.
 */

const TYPE_ICON = { video: 'video', image: 'image', voice: 'mic', text: 'text', static: 'image' };
const TYPE_LABEL = { video: 'Video', image: 'Ảnh', voice: 'Ghi âm', text: 'Văn bản', static: 'Tin nhắn tĩnh' };

function summaryLine(type, duration) {
  const label = TYPE_LABEL[type] || 'Nội dung';
  return duration ? `${label} · ${duration}s` : label;
}

/** Một dòng trong danh sách 3 bước của popup */
function StepRow({ state, label }) {
  let dot;
  if (state === 'done') {
    dot = <CircleIcon size={20} bg="var(--success-bg)" color="var(--success-fill)" icon="check" iconSize={16} />;
  } else if (state === 'now') {
    dot = <CircleIcon size={20} bg="var(--chip-accent-bg)" color="var(--chip-accent-fg)" icon="up" iconSize={16} />;
  } else {
    dot = (
      <span
        className="sl-circle"
        style={{ width: 20, height: 20, background: 'var(--neutral-0)', border: '0.5px solid var(--neutral-100)' }}
      />
    );
  }
  return (
    <div className={`sl-step-row sl-step-row--${state}`}>
      {dot}
      <span>{label}</span>
    </div>
  );
}

const EncodingProgress = ({
  phase,
  progress,
  type,
  duration,
  fileName,
  boxName,
  onHome,
  onRetry,
  onSendAnother,
  onLeave,
  errorText,
}) => {
  // ---- 04 / 05: màn kết quả, không có appbar theo thiết kế ----
  if (phase === 'done' || phase === 'error') {
    const ok = phase === 'done';
    return (
      <Screen>
        <Body center>
          <CircleIcon
            size={88}
            bg={ok ? 'var(--success-bg)' : 'var(--error-bg)'}
            color={ok ? 'var(--success-fill)' : 'var(--error-fill)'}
            icon={ok ? 'check' : 'alert'}
            iconSize={40}
            sw={2.5}
          />

          <h1 className="sl-title">{ok ? 'Đã gửi tới hộp' : 'Chưa gửi được'}</h1>

          <p className="sl-body">
            {ok
              ? `${boxName} sẽ hiện nội dung này trong lần thức dậy kế tiếp — thường trong vòng 5 phút.`
              : 'Việc gửi dừng giữa chừng. Chưa có gì tới hộp, nội dung của bạn vẫn còn ở đây.'}
          </p>

          {!ok && (
            <div className="sl-reason">
              {errorText || 'Mất kết nối khi đang tải lên. Kiểm tra mạng rồi thử lại.'}
            </div>
          )}

          <div className="sl-listcard">
            <Chip icon={TYPE_ICON[type] || 'chat'} />
            <div className="sl-listcard__mid">
              <span className="sl-label-s">{summaryLine(type, duration)}</span>
              <span className="sl-caption">
                {boxName} · {ok ? 'đã gửi' : 'còn giữ trên máy này'}
              </span>
            </div>
          </div>

          <Actions>
            {ok ? (
              <>
                <Button kind="pri" onClick={onHome}>Về trang chủ</Button>
                <Button kind="gho" onClick={onSendAnother}>Gửi tiếp</Button>
              </>
            ) : (
              <>
                <Button kind="pri" onClick={onRetry}>Thử lại</Button>
                <Button kind="gho" onClick={onHome}>Về trang chủ</Button>
              </>
            )}
          </Actions>
        </Body>
      </Screen>
    );
  }

  // ---- 03: popup đè lên nền bước 2 ----
  const uploading = phase === 'uploading';
  return (
    <Screen>
      <AppBar step="Bước 3/3" />
      <Body>
        <div className="sl-hdr">
          <h1 className="sl-title">Gửi {(TYPE_LABEL[type] || '').toLowerCase()}</h1>
          <div className="sl-hdr__to">
            <Icon name="heart" size={16} />
            <span className="sl-caption">{boxName}</span>
          </div>
        </div>

        <div className="sl-card sl-card--center" style={{ minHeight: 180 }}>
          <Chip icon={TYPE_ICON[type] || 'chat'} />
          <span className="sl-label-s">{fileName || summaryLine(type, duration)}</span>
          {duration ? <span className="sl-caption">{duration}s</span> : null}
        </div>
      </Body>

      <Modal>
        <div style={{ display: 'flex', flexDirection: 'column', gap: 4 }}>
          <span className="sl-heading">Đang gửi tới hộp của bạn</span>
          <span className="sl-body">
            {uploading ? 'Đang tải lên đám mây...' : 'Đang xử lý hình ảnh & âm thanh...'}
          </span>
        </div>

        <div className="sl-progress">
          <div className="sl-track">
            <div className="sl-track__bar" style={{ width: `${progress}%` }} />
          </div>
          <span className="sl-caption-s">{progress}%</span>
        </div>

        <div className="sl-steps">
          <StepRow state={uploading ? 'done' : 'now'} label="Xử lý hình ảnh và âm thanh" />
          <StepRow state={uploading ? 'now' : 'wait'} label="Tải lên đám mây" />
          <StepRow state="wait" label="Hộp nhận trong lần thức dậy kế tiếp" />
        </div>

        <p className="sl-caption">
          Hộp thức dậy 5 phút một lần để kiểm tra tin mới. Bạn có thể rời màn hình này — việc gửi vẫn tiếp tục.
        </p>

        {/* Đúng như dòng chú thích ngay trên: nút này chỉ rời màn hình,
            không huỷ được lượt tải đang chạy. */}
        <Button kind="gho" onClick={onLeave}>Để sau</Button>
      </Modal>
    </Screen>
  );
};

export default EncodingProgress;
