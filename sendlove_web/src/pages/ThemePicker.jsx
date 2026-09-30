import React, { useEffect, useState } from 'react';
import { useParams, useNavigate } from 'react-router-dom';
import { useAuth } from '../context/AuthContext';
import { getTheme } from '../api/theme';
import { getBoxDetails } from '../api/box';
import Icon from '../components/ui/Icon';
import { Screen, AppBar, Body, Actions, Header, Button } from '../components/ui/Screen';
import BoxScreen, { MiniScreen } from '../components/theme/BoxScreen';
import { DEFAULT_BG_URL, DEFAULT_WIDGETS, FONT_FAMILIES, PRESETS, toEditorWidgets } from '../theme/layout';
import { loadDefaultBackground } from '../utils/rgb565';
import { ensureWebFont } from '../utils/vlw';

/**
 * Màn 19 "theme picker".
 *
 * Nguồn: GET /boxes/:boxId/theme (bản đã lưu trên tài khoản, có thể null) +
 * các mẫu dựng sẵn trong theme/layout.js. Chọn một dòng thì xem trước đổi
 * theo — mọi mẫu đều là bố cục thật nên xem trước được.
 *
 * "Bản nháp" đi sang editor/send qua location.state:
 *   { name, widgets, bg: null | { path, url } | { bytes, previewUrl } }
 */
export default function ThemePicker() {
  const { boxId } = useParams();
  const navigate = useNavigate();
  const { profile } = useAuth();
  const [saved, setSaved] = useState(undefined); // undefined = đang tải, null = chưa lưu
  const [error, setError] = useState(null);
  const [selected, setSelected] = useState(null);
  const [defaultBg, setDefaultBg] = useState(null);
  const [boxThemeRev, setBoxThemeRev] = useState(undefined);

  // Nền mặc định + phông để xem trước mẫu; status.theme_rev = bản hộp đang hiển thị.
  useEffect(() => {
    loadDefaultBackground(DEFAULT_BG_URL).then(setDefaultBg).catch(() => {});
    FONT_FAMILIES.forEach((f) => ensureWebFont(f.family, f.weight).catch(() => {}));
    getBoxDetails(boxId).then((r) => setBoxThemeRev(r.data?.status?.theme_rev ?? null)).catch(() => {});
  }, [boxId]);

  useEffect(() => {
    let alive = true;
    getTheme(boxId)
      .then((res) => {
        if (!alive) return;
        setSaved(res.data || null);
        setSelected(res.data ? 'saved' : 'default');
      })
      .catch((err) => {
        if (!alive) return;
        setSaved(null);
        setSelected('default');
        setError(err.response?.data?.error?.message || 'Không đọc được giao diện đã lưu.');
      });
    return () => { alive = false; };
  }, [boxId]);

  const options = [
    ...(saved ? [{
      id: 'saved', name: saved.theme_name, what: 'Đã lưu trên tài khoản',
      widgets: toEditorWidgets(saved.widgets),
      bg: saved.background ? { path: saved.background, url: saved.background_url } : null,
    }] : []),
    ...PRESETS.map((p) => ({ ...p, widgets: toEditorWidgets(p.widgets), bg: p.defaultBg ? defaultBg : null })),
  ];
  const current = options.find((o) => o.id === selected);
  const draft = current && { name: current.name, widgets: current.widgets, bg: current.bg };

  return (
    <Screen>
      <AppBar onBack={() => navigate(`/box/${boxId}/receiver/config`)} />
      <Body>
        <Header title="Giao diện màn hình hộp" to={profile?.boxes_list?.[boxId]?.box_name || `Hộp ${boxId}`} />

        {/* Hộp tải gói theme về thẻ nhớ rồi chép sang bộ nhớ trong; status.theme_rev = bản đang hiện. */}
        {saved && saved.rev ? (
          <div className="sl-note">
            <Icon name="sync" size={16} />
            <span>
              {boxThemeRev === saved.rev
                ? 'Hộp đang hiển thị giao diện đã lưu.'
                : 'Hộp sẽ đổi sang giao diện đã lưu ở lần đồng bộ kế tiếp (hộp đang ngủ thì tới 5 phút).'}
            </span>
          </div>
        ) : saved ? (
          <div className="sl-note sl-note--warn">
            <Icon name="alert" size={16} />
            <span>Giao diện này lưu bằng bản web cũ. Hãy mở và lưu lại một lần để hộp tải được.</span>
          </div>
        ) : null}

        {error && <div className="sl-reason">{error}</div>}

        <div className="sl-card" style={{ padding: 'var(--sp-3)', gap: 10 }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: 'var(--sp-2)' }}>
            <Icon name="palette" size={16} style={{ color: 'var(--text-accent)' }} />
            <span className="sl-caption" style={{ flex: 1, fontWeight: 500 }}>Xem trước 240 × 240</span>
            <span className="sl-caption" style={{ fontWeight: 600, color: 'var(--caramel-900)' }}>
              {current?.name || '…'}
            </span>
          </div>
          <BoxScreen widgets={current?.widgets || DEFAULT_WIDGETS} background={current?.bg?.url || current?.bg?.previewUrl} />
        </div>

        <div style={{ display: 'flex', alignItems: 'center', gap: 'var(--sp-2)' }}>
          <span className="sl-label" style={{ flex: 1 }}>Giao diện</span>
          <span className="sl-caption" style={{ fontWeight: 500 }}>{saved === undefined ? '…' : options.length}</span>
        </div>

        {saved === undefined ? (
          <span className="sl-body">Đang tải…</span>
        ) : (
          <div style={{ display: 'flex', flexDirection: 'column', gap: 'var(--sp-2)' }}>
            {options.map((t) => {
              const on = t.id === selected;
              return (
                <button
                  key={t.id} type="button" className="sl-listcard" onClick={() => setSelected(t.id)}
                  aria-pressed={on}
                  style={{
                    padding: '10px var(--sp-4)', cursor: 'pointer',
                    borderColor: on ? 'var(--rose-400)' : 'var(--line-card)',
                    borderWidth: on ? 1 : 0.5,
                  }}
                >
                  <MiniScreen widgets={t.widgets} background={t.bg?.url || t.bg?.previewUrl} />
                  <div className="sl-listcard__mid">
                    <span className="sl-label-s" style={{ fontSize: 15 }}>{t.name}</span>
                    <span className="sl-caption">{t.what}</span>
                  </div>
                  {on
                    ? <Icon name="check" size={20} style={{ color: 'var(--text-accent)' }} />
                    : <Icon name="chevron" size={16} style={{ color: 'var(--neutral-400)' }} />}
                </button>
              );
            })}
          </div>
        )}

        <Actions>
          <Button kind="pri" disabled={!draft}
            onClick={() => navigate(`/box/${boxId}/receiver/theme/edit`, { state: draft })}>
            Sửa giao diện này
          </Button>
          <Button kind="gho" disabled={!draft || selected === 'saved'}
            onClick={() => navigate(`/box/${boxId}/receiver/theme/send`, { state: draft })}>
            Dùng luôn mẫu này
          </Button>
        </Actions>
      </Body>
    </Screen>
  );
}
