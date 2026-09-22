import React, { useState } from 'react';
import { useParams, useNavigate, useLocation, Navigate } from 'react-router-dom';
import Icon from '../components/ui/Icon';
import { Screen, AppBar, Body, Actions, Header, Button, Tips, CircleIcon, Modal } from '../components/ui/Screen';
import BoxScreen from '../components/theme/BoxScreen';
import { saveTheme, uploadBackground } from '../api/theme';
import { toApiWidgets } from '../theme/layout';

/**
 * Màn 21 "theme send" — lưu bản nháp lên tài khoản.
 *
 *   1. Có ảnh nền mới → POST /theme/background lấy signed POST, tải 115.200 B lên
 *   2. PUT /theme {theme_name, widgets, background} → backend lọc lại widgets,
 *      ghi boxes/{id}/config/theme và bật flags/theme_flag
 *
 * Hộp chỉ áp dụng khi firmware đọc theme_flag — bản hiện tại CHƯA đọc
 * (memory sendlove-fw-todo-tu-fe mục 3). Nói thẳng điều đó, không hứa "hộp
 * sẽ nhận ở lần thức dậy kế tiếp".
 */
export default function ThemeSend() {
  const { boxId } = useParams();
  const navigate = useNavigate();
  const { state: draft } = useLocation();
  const [name, setName] = useState(draft?.name || 'Giao diện của tôi');
  const [phase, setPhase] = useState('idle'); // idle | uploading | saving | done
  const [progress, setProgress] = useState(0);
  const [error, setError] = useState(null);

  // Mở thẳng URL mà không có bản nháp → quay về trình sửa.
  if (!draft?.widgets) return <Navigate to={`/box/${boxId}/receiver/theme/edit`} replace />;

  const bg = draft.bg;
  const hasNewBg = !!bg?.bytes;
  const busy = phase === 'uploading' || phase === 'saving';
  const nameOk = name.trim().length >= 1 && name.trim().length <= 40;

  const submit = async () => {
    setError(null);
    try {
      let background = bg?.path || null;
      if (hasNewBg) {
        setPhase('uploading');
        setProgress(0);
        background = await uploadBackground(boxId, bg.bytes, (p) => setProgress(Math.round(p)));
      }
      setPhase('saving');
      await saveTheme(boxId, { theme_name: name.trim(), widgets: toApiWidgets(draft.widgets), background });
      setPhase('done');
    } catch (err) {
      setPhase('idle');
      setError(err.response?.data?.error?.message || 'Không lưu được giao diện. Kiểm tra kết nối rồi thử lại.');
    }
  };

  return (
    <Screen>
      <AppBar onBack={() => navigate(`/box/${boxId}/receiver/theme/edit`, { state: draft })} />
      <Body>
        <Header title="Lưu giao diện" to={`${draft.widgets.length} widget${bg ? ' · có ảnh nền' : ''}`} />

        <div className="sl-card" style={{ padding: 10 }}>
          <BoxScreen widgets={draft.widgets} background={bg?.url || bg?.previewUrl} />
        </div>

        <label className="sl-field">
          <span className="sl-label">Tên giao diện</span>
          <input className="sl-input" value={name} maxLength={40} onChange={(e) => setName(e.target.value)} disabled={busy} />
        </label>

        <div className="sl-card" style={{ padding: '14px var(--sp-4)', gap: 'var(--sp-3)' }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: 'var(--sp-2)' }}>
            <Icon name="layers" size={18} style={{ color: 'var(--caramel-700)' }} />
            <span className="sl-label-s" style={{ flex: 1 }}>Bố cục</span>
            <span className="sl-caption" style={{ fontWeight: 500 }}>{draft.widgets.length} widget · ~1 KB</span>
          </div>
          <span style={{ height: 0.5, background: 'var(--neutral-100)' }} />
          <div style={{ display: 'flex', alignItems: 'center', gap: 'var(--sp-2)' }}>
            <Icon name="image" size={18} style={{ color: 'var(--caramel-700)' }} />
            <span className="sl-label-s" style={{ flex: 1 }}>Ảnh nền</span>
            <span className="sl-caption" style={{ fontWeight: 500 }}>
              {hasNewBg ? 'Ảnh mới · 115,2 KB' : bg ? 'Giữ ảnh đã lưu' : 'Nền dựng sẵn'}
            </span>
          </div>
        </div>

        <div className="sl-note sl-note--warn">
          <Icon name="alert" size={16} />
          <span>
            Giao diện được lưu lên tài khoản ngay. Hộp chỉ đổi màn chờ khi chạy bản firmware đọc
            được giao diện từ tài khoản — bản hiện tại vẫn hiện giao diện dựng sẵn.
          </span>
        </div>

        {error && <div className="sl-reason">{error}</div>}

        {phase === 'uploading' && (
          <div className="sl-progress">
            <div className="sl-track"><div className="sl-track__bar" style={{ width: `${progress}%` }} /></div>
            <span className="sl-caption-s">Đang tải ảnh nền {progress}%</span>
          </div>
        )}

        <Tips>Lưu đè lên giao diện cũ của hộp này. Giao diện cũ không được giữ lại.</Tips>

        <Actions>
          <Button kind="pri" onClick={submit} disabled={busy || !nameOk}>
            {phase === 'uploading' ? 'Đang tải ảnh nền…' : phase === 'saving' ? 'Đang lưu…' : 'Lưu giao diện'}
          </Button>
          <Button kind="gho" disabled={busy}
            onClick={() => navigate(`/box/${boxId}/receiver/theme/edit`, { state: { ...draft, name } })}>
            Sửa tiếp
          </Button>
        </Actions>
      </Body>

      {phase === 'done' && (
        <Modal>
          <span style={{ alignSelf: 'center' }}>
            <CircleIcon size={72} bg="var(--success-bg)" color="var(--success-fill)" icon="check" iconSize={32} sw={2} />
          </span>
          <span className="sl-heading" style={{ textAlign: 'center' }}>Đã lưu “{name.trim()}”</span>
          <span className="sl-body" style={{ textAlign: 'center' }}>
            Giao diện nằm trên tài khoản của hộp. Khi hộp chạy bản firmware hỗ trợ, nó sẽ tự tải về ở
            lần đồng bộ kế tiếp.
          </span>
          <Button kind="pri" onClick={() => navigate(`/box/${boxId}/receiver/theme`, { replace: true })}>
            Về danh sách giao diện
          </Button>
        </Modal>
      )}
    </Screen>
  );
}
