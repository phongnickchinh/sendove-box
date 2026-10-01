import React, { useEffect, useState } from 'react';
import { useParams, useNavigate, useLocation, Navigate } from 'react-router-dom';
import Icon from '../components/ui/Icon';
import { Screen, AppBar, Body, Actions, Header, Button, Tips, CircleIcon, Modal } from '../components/ui/Screen';
import { ExactPreview } from '../components/theme/BoxScreen';
import { saveTheme, uploadBackground, uploadFont } from '../api/theme';
import { toApiWidgets } from '../theme/layout';
import { buildThemeFonts } from '../utils/themePack';

/**
 * Theme send — saves the theme package to the account (SD-card design, firmware MEMORY.md §28).
 *
 *   1. Subset fonts: each text widget type using a web font → one VLW file with
 *      exactly the needed characters (utils/vlw.js). The preview below is drawn
 *      from THESE very files (firmware MEMORY.md §29, proposal #10).
 *   2. Upload: the new background (or the default one), 115,200 B, + the font files.
 *   3. PUT /theme {theme_name, widgets, background, fonts} → the backend measures
 *      size + crc32 of each file, bumps rev and sets theme_flag. On its next sync
 *      the box downloads the package to the card, copies it to flash and redraws.
 */
export default function ThemeSend() {
  const { boxId } = useParams();
  const navigate = useNavigate();
  const { state: draft } = useLocation();
  const [name, setName] = useState(draft?.name || 'Giao diện của tôi');
  const [phase, setPhase] = useState('preparing'); // preparing | idle | uploading | saving | done
  const [progress, setProgress] = useState(0);
  const [error, setError] = useState(null);
  const [fonts, setFonts] = useState(null);
  const [bgBytes, setBgBytes] = useState(null);

  const bg = draft?.bg;

  // Subset the fonts + get the background bytes for a box-accurate preview.
  useEffect(() => {
    if (!draft?.widgets) return undefined;
    let alive = true;
    (async () => {
      try {
        const [f, bytes] = await Promise.all([
          buildThemeFonts(draft.widgets),
          bg?.bytes ? Promise.resolve(bg.bytes)
            : bg?.url ? fetch(bg.url).then((r) => r.arrayBuffer()).then((b) => new Uint8Array(b))
            : Promise.resolve(null),
        ]);
        if (!alive) return;
        setFonts(f);
        setBgBytes(bytes);
        setPhase('idle');
      } catch (err) {
        if (!alive) return;
        setError(err.message || 'Không cắt được phông chữ. Kiểm tra kết nối rồi thử lại.');
        setPhase('idle');
      }
    })();
    return () => { alive = false; };
  }, [draft, bg]);

  // Opened directly without a draft → go back to the editor.
  if (!draft?.widgets) return <Navigate to={`/box/${boxId}/receiver/theme/edit`} replace />;

  const hasNewBg = !!bg?.bytes;
  const busy = phase !== 'idle' && phase !== 'done';
  const nameOk = name.trim().length >= 1 && name.trim().length <= 40;
  const fontKB = fonts ? Object.values(fonts).reduce((s, f) => s + f.bytes.length, 0) / 1024 : 0;

  const submit = async () => {
    setError(null);
    try {
      setPhase('uploading');
      setProgress(0);
      let background = bg?.path || null;
      if (hasNewBg) background = await uploadBackground(boxId, bg.bytes, (p) => setProgress(Math.round(p * 0.8)));
      const fontPaths = {};
      for (const [key, f] of Object.entries(fonts || {})) {
        fontPaths[key] = await uploadFont(boxId, f.bytes);
      }
      setProgress(100);
      setPhase('saving');
      await saveTheme(boxId, {
        theme_name: name.trim(), widgets: toApiWidgets(draft.widgets), background, fonts: fontPaths,
      });
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

        <div className="sl-card" style={{ padding: 10, gap: 8 }}>
          {phase === 'preparing'
            ? <span className="sl-body" style={{ alignSelf: 'center' }}>Đang cắt phông chữ…</span>
            : <ExactPreview widgets={draft.widgets} fonts={fonts} bgBytes={bgBytes} />}
          <span className="sl-caption" style={{ alignSelf: 'center' }}>
            Vẽ bằng đúng ảnh nền và phông sẽ gửi xuống hộp
          </span>
        </div>

        <label className="sl-field">
          <span className="sl-label">Tên giao diện</span>
          <input className="sl-input" value={name} maxLength={40} onChange={(e) => setName(e.target.value)} disabled={busy} />
        </label>

        <div className="sl-card" style={{ padding: '14px var(--sp-4)', gap: 'var(--sp-3)' }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: 'var(--sp-2)' }}>
            <Icon name="layers" size={18} style={{ color: 'var(--caramel-700)' }} />
            <span className="sl-label-s" style={{ flex: 1 }}>Bố cục + phông chữ</span>
            <span className="sl-caption" style={{ fontWeight: 500 }}>
              {draft.widgets.length} widget · {fonts ? `${fontKB.toFixed(1)} KB phông` : '…'}
            </span>
          </div>
          <span style={{ height: 0.5, background: 'var(--neutral-100)' }} />
          <div style={{ display: 'flex', alignItems: 'center', gap: 'var(--sp-2)' }}>
            <Icon name="image" size={18} style={{ color: 'var(--caramel-700)' }} />
            <span className="sl-label-s" style={{ flex: 1 }}>Ảnh nền</span>
            <span className="sl-caption" style={{ fontWeight: 500 }}>
              {bg?.isDefault ? 'Nền mặc định · 115,2 KB' : hasNewBg ? 'Ảnh mới · 115,2 KB' : bg ? 'Giữ ảnh đã lưu' : 'Nền đen'}
            </span>
          </div>
        </div>

        <div className="sl-note">
          <Icon name="sync" size={16} />
          <span>
            Hộp tải giao diện về thẻ nhớ ở lần đồng bộ kế tiếp (hộp đang ngủ thì tới 5 phút), rồi
            tự đổi màn chờ. Thẻ nhớ có hỏng sau đó thì hộp vẫn giữ giao diện này.
          </span>
        </div>

        {error && <div className="sl-reason">{error}</div>}

        {phase === 'uploading' && (
          <div className="sl-progress">
            <div className="sl-track"><div className="sl-track__bar" style={{ width: `${progress}%` }} /></div>
            <span className="sl-caption-s">Đang tải lên {progress}%</span>
          </div>
        )}

        <Tips>Lưu đè lên giao diện cũ của hộp này. Giao diện cũ không được giữ lại.</Tips>

        <Actions>
          <Button kind="pri" onClick={submit} disabled={busy || !nameOk || !fonts}>
            {phase === 'uploading' ? 'Đang tải lên…' : phase === 'saving' ? 'Đang lưu…' : 'Lưu giao diện'}
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
            Hộp sẽ tải giao diện ở lần đồng bộ kế tiếp và đổi màn chờ ngay sau đó.
          </span>
          <Button kind="pri" onClick={() => navigate(`/box/${boxId}/receiver/theme`, { replace: true })}>
            Về danh sách giao diện
          </Button>
        </Modal>
      )}
    </Screen>
  );
}
