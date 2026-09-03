import React, { useState } from 'react';
import { useParams, useNavigate } from 'react-router-dom';
import Icon from '../components/ui/Icon';
import { Screen, AppBar, Body, Actions, Header, Button, Tips, CircleIcon, Modal } from '../components/ui/Screen';
import { FILES, NEEDS_FW } from '../theme/layout';

/**
 * Màn 21 "theme send".
 *
 * Màn này nói rõ HỢP ĐỒNG ĐỀ XUẤT, vì hôm nay chưa có gì trong số này:
 *   - chưa có route nhận theme (NetworkManager chỉ có provisioning AP)
 *   - IStorageProvider.h không có API ghi file tuỳ ý
 *   - SDStorageProvider.cpp:5-14 mới chỉ sinh đường dẫn /media/*.bin
 * Nên phải liệt kê ĐÚNG tên file và ĐÚNG dung lượng: đó là phần firmware sẽ
 * phải dựng.
 */

const STEPS = [
  'Giao diện được xếp hàng trên tài khoản của bạn.',
  'Hộp lấy về ở lần thức dậy kế tiếp — cùng chuyến nó đi lấy tin nhắn.',
  'Cả hai file nằm lên thẻ, hộp nạp lại bố cục. Không cần khởi động lại.',
];

export default function ThemeSend() {
  const { boxId } = useParams();
  const navigate = useNavigate();
  const [queued, setQueued] = useState(false);

  return (
    <Screen>
      <AppBar onBack={() => navigate(`/box/${boxId}/receiver/theme/edit`)} />
      <Body>
        <Header title="Gửi giao diện xuống hộp" to={`Hộp ${boxId}`} />

        {/* --- những gì thật sự được ghi lên thẻ --- */}
        <div className="sl-card" style={{ padding: '14px var(--sp-4)', gap: 'var(--sp-3)' }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: 'var(--sp-2)' }}>
            <Icon name="sd" size={20} style={{ color: 'var(--caramel-700)' }} />
            <span className="sl-label-s" style={{ flex: 1 }}>Ghi lên thẻ nhớ</span>
            <span className="sl-caption" style={{ fontWeight: 500 }}>116,4 KB</span>
          </div>

          {FILES.map((f, i) => (
            <React.Fragment key={f.path}>
              {i > 0 && <span style={{ height: 0.5, background: 'var(--neutral-100)' }} />}
              <div style={{ display: 'flex', flexDirection: 'column', gap: 2 }}>
                <span style={{ display: 'flex', alignItems: 'center', gap: 'var(--sp-2)' }}>
                  <span className="sl-label-s" style={{ flex: 1 }}>{f.path}</span>
                  <span className="sl-caption" style={{ fontWeight: 500 }}>{f.size}</span>
                </span>
                <span className="sl-caption">{f.what}</span>
              </div>
            </React.Fragment>
          ))}
        </div>

        <div className="sl-note sl-note--warn">
          <Icon name="alert" size={16} />
          <span>
            Hộp đang chạy firmware cũ nên vẫn vẽ giao diện dựng sẵn bên trong.
            Đọc được hai file này cần firmware {NEEDS_FW}.
          </span>
        </div>

        {/* --- đường đi của file --- */}
        <div style={{ display: 'flex', flexDirection: 'column', gap: 10, padding: '14px var(--sp-4)', borderRadius: 'var(--r-md)', background: 'var(--caramel-50)' }}>
          <span className="sl-label-s">Đường đi tới hộp</span>
          {STEPS.map((s, i) => (
            <span key={i} style={{ display: 'flex', gap: 'var(--sp-2)', alignItems: 'flex-start' }}>
              <span style={{
                flex: '0 0 auto', width: 20, height: 20, borderRadius: 999,
                background: 'var(--caramel-100)', color: 'var(--caramel-800)',
                display: 'inline-flex', alignItems: 'center', justifyContent: 'center',
                fontSize: 12, fontWeight: 600,
              }}>{i + 1}</span>
              <span style={{ fontSize: 12, lineHeight: 1.5, color: 'var(--caramel-700)' }}>{s}</span>
            </span>
          ))}
        </div>

        <Tips>
          Không có gì bị ghi đè cho tới khi cả hai file tới nơi. Truyền dở dang thì
          giao diện cũ vẫn chạy.
        </Tips>

        <Actions>
          <Button kind="pri" onClick={() => setQueued(true)}>Gửi xuống hộp</Button>
          <Button kind="gho" onClick={() => navigate(`/box/${boxId}/receiver/theme`)}>
            Lưu mà chưa gửi
          </Button>
        </Actions>
      </Body>

      {queued && (
        <Modal>
          <span style={{ alignSelf: 'center' }}>
            <CircleIcon size={72} bg="var(--rose-50)" color="var(--rose-400)" icon="download" iconSize={32} sw={2} />
          </span>
          <span className="sl-heading" style={{ textAlign: 'center' }}>Đã xếp hàng — chờ hộp thức dậy</span>
          <span className="sl-body" style={{ textAlign: 'center' }}>
            Chạm vào hộp để đánh thức ngay, hoặc cứ để đó — giao diện sẽ có ở lần sau.
          </span>
          <Tips>
            Nếu hộp không có thẻ nhớ, việc truyền dừng ở đây và giao diện dựng sẵn vẫn chạy.
          </Tips>
          <Button kind="gho" onClick={() => setQueued(false)}>Đóng</Button>
        </Modal>
      )}
    </Screen>
  );
}
