import React, { useEffect, useState } from 'react';
import { useParams, useNavigate, useLocation } from 'react-router-dom';
import Icon from '../components/ui/Icon';
import { Screen, AppBar, Body, Actions, Header, Button, Modal } from '../components/ui/Screen';
import BoxScreen from '../components/theme/BoxScreen';
import { getTheme } from '../api/theme';
import { imageToRgb565, loadDefaultBackground } from '../utils/rgb565';
import { ensureWebFont } from '../utils/vlw';
import {
  ALIGNS, BUILTIN_FONTS, DATE_FORMATS, DEFAULT_BG_URL, DEFAULT_WIDGETS, FONT_FAMILIES, MAX_WIDGETS,
  PX_RANGE, SCREEN, VLW_KEY, WIDGET_TYPES, isVlw, newWidget, sharedFontIssue, toEditorWidgets, widgetProblem,
} from '../theme/layout';

/**
 * Theme editor.
 *
 * Layout: the preview is PINNED to the top (sticky) — tap to select, drag to
 * move a widget — with the property panel right below it, so position edits
 * are visible while editing.
 *
 * A field appears in the property panel only if LayoutEngine.cpp ACTUALLY
 * reads it for that widget type (WIDGET_TYPES in theme/layout.js):
 *   font    -> a web font family (subset to VLW on send) or a firmware built-in font
 *   format  -> clock_date: LayoutEngine::formatDate (+ locale vi/en)
 *   color   -> hexToColor, exactly 7 chars #RRGGBB
 *   align   -> drawTextWidget: center / right / anything else = left
 *   x,y,w,h -> drawBackgroundPatch clears exactly the w×h box before drawing
 *
 * INTENTIONALLY ABSENT: image widgets (the firmware can't draw them) and a
 * battery color (drawBatteryIcon ignores cfg.color).
 *
 * The draft comes from ThemePicker through location.state; opened directly by
 * URL, it loads the saved theme, or the default layout if there is none.
 */
export default function ThemeEditor() {
  const { boxId } = useParams();
  const navigate = useNavigate();
  const { state } = useLocation();

  const [name, setName] = useState(state?.name || 'Mặc định');
  const [widgets, setWidgets] = useState(state?.widgets || DEFAULT_WIDGETS);
  const [bg, setBg] = useState(state?.bg || null);
  const [selectedId, setSelectedId] = useState(null);
  const [adding, setAdding] = useState(false);
  const [bgBusy, setBgBusy] = useState(false);
  const [bgError, setBgError] = useState(null);
  const [touched, setTouched] = useState(false); // has changes not yet taken to the save step
  const [confirmLeave, setConfirmLeave] = useState(false);

  // Load the font families so the preview shows the real glyphs (the same fonts are subset to VLW).
  useEffect(() => {
    FONT_FAMILIES.forEach((f) => ensureWebFont(f.family, f.weight).catch(() => {}));
  }, []);

  // Opened directly at /theme/edit (not via the picker): load the saved theme if any.
  useEffect(() => {
    if (state) return undefined;
    let alive = true;
    getTheme(boxId).then((res) => {
      if (!alive || !res.data) return;
      setName(res.data.theme_name);
      setWidgets(toEditorWidgets(res.data.widgets));
      if (res.data.background) setBg({ path: res.data.background, url: res.data.background_url });
    }).catch(() => {});
    return () => { alive = false; };
  }, [boxId, state]);

  const sel = widgets.find((w) => w.id === selectedId);
  const meta = sel ? WIDGET_TYPES[sel.type] : null;
  const patchWidget = (id, fields) => {
    setTouched(true);
    setWidgets((prev) => prev.map((w) => (w.id === id ? { ...w, ...fields } : w)));
  };
  const patch = (fields) => patchWidget(selectedId, fields);

  const tooSmall = sel && meta && (sel.w < meta.min.w || sel.h < meta.min.h);
  const badHex = sel && meta?.color && !/^#[0-9A-Fa-f]{6}$/.test(sel.color || '');
  const problems = widgets.map(widgetProblem).filter(Boolean);
  const selProblem = sel ? widgetProblem(sel) : null;

  const addWidget = (type) => {
    const w = newWidget(type, widgets);
    setTouched(true);
    setWidgets((prev) => [...prev, w]);
    setAdding(false);
    setSelectedId(w.id);
  };

  const removeSelected = () => {
    setTouched(true);
    setWidgets((prev) => prev.filter((w) => w.id !== selectedId));
    setSelectedId(null);
  };

  const changeBg = async (load) => {
    setBgBusy(true);
    setBgError(null);
    try {
      setBg(await load());
      setTouched(true);
    } catch {
      setBgError('Không đọc được ảnh này. Thử một ảnh JPG hoặc PNG khác.');
    } finally {
      setBgBusy(false);
    }
  };

  const pickBackground = (e) => {
    const file = e.target.files[0];
    e.target.value = '';
    if (file) changeBg(() => imageToRgb565(file));
  };

  const fontIssue = sharedFontIssue(widgets);
  const bgUrl = bg?.url || bg?.previewUrl || null;
  const goSend = () => navigate(`/box/${boxId}/receiver/theme/send`, { state: { name, widgets, bg } });
  const goBack = () => navigate(`/box/${boxId}/receiver/theme`);

  /* Number fields may be cleared while typing (instead of snapping to 0). An
     empty field makes widgetProblem report an error until a number is entered. */
  const numField = (k) => (
    <label key={k} className="sl-editor__num">
      <span className="sl-caption">{k.toUpperCase()}</span>
      <input
        type="number" inputMode="numeric" min={0} max={SCREEN} step={1}
        value={sel[k]}
        disabled={meta.fixedSize && (k === 'w' || k === 'h')}
        onChange={(e) => patch({ [k]: e.target.value === '' ? '' : Math.round(Number(e.target.value)) })}
      />
    </label>
  );

  /* 1px nudge buttons — finger dragging isn't pixel-accurate. */
  const nudge = (dx, dy) => {
    const clamp = (v, max) => Math.max(0, Math.min(max, v));
    patch({
      x: clamp((Number(sel.x) || 0) + dx, SCREEN - (Number(sel.w) || 0)),
      y: clamp((Number(sel.y) || 0) + dy, SCREEN - (Number(sel.h) || 0)),
    });
  };

  return (
    <Screen>
      <AppBar onBack={() => (touched ? setConfirmLeave(true) : goBack())} />
      <Body>
        <Header title="Sửa giao diện" to={name} />

        {/* --- preview, pinned to the top while the property panel scrolls --- */}
        <div className="sl-editor__stage">
          <div className="sl-editor__screen">
            <BoxScreen
              widgets={widgets} selectedId={selectedId} onSelect={setSelectedId}
              onMove={(id, pos) => patchWidget(id, pos)} showBoxes background={bgUrl}
            />
          </div>
          <div className="sl-editor__chips" role="toolbar" aria-label="Widget trên màn">
            {widgets.map((w) => {
              const bad = !!widgetProblem(w);
              return (
                <button key={w.id} type="button"
                  className={`sl-editor__chip${bad ? ' sl-editor__chip--bad' : ''}`}
                  aria-pressed={w.id === selectedId}
                  onClick={() => setSelectedId(w.id === selectedId ? null : w.id)}>
                  <Icon name={bad ? 'alert' : w.icon} size={14} />
                  {w.label}
                </button>
              );
            })}
            <button type="button" className="sl-editor__chip sl-editor__chip--add"
              disabled={widgets.length >= MAX_WIDGETS} onClick={() => setAdding(true)}
              aria-label={widgets.length >= MAX_WIDGETS ? `Tối đa ${MAX_WIDGETS} widget` : 'Thêm widget'}>
              <Icon name="plus" size={14} />
              {widgets.length >= MAX_WIDGETS ? `${MAX_WIDGETS}/${MAX_WIDGETS}` : 'Thêm'}
            </button>
          </div>
        </div>

        {/* --- property panel of the selected widget --- */}
        {sel && meta ? (
          <div className="sl-card sl-editor__panel">
            <div style={{ display: 'flex', alignItems: 'center', gap: 'var(--sp-2)' }}>
              <Icon name={sel.icon} size={20} style={{ color: 'var(--text-accent)' }} />
              <span className="sl-heading" style={{ flex: 1 }}>{sel.label}</span>
              <button type="button" className="sl-iconbtn sl-iconbtn--44" onClick={removeSelected}
                aria-label={`Xoá widget ${sel.label}`} style={{ color: 'var(--error-text)' }}>
                <Icon name="trash" size={20} />
              </button>
              <button type="button" className="sl-iconbtn sl-iconbtn--44" onClick={() => setSelectedId(null)} aria-label="Bỏ chọn">
                <Icon name="x" size={20} />
              </button>
            </div>

            {/* Position + size: number fields and 1px nudge buttons */}
            <div className="sl-field">
              <span className="sl-label">Vị trí và kích thước</span>
              <div className="sl-editor__pos">
                {['x', 'y', 'w', 'h'].map(numField)}
              </div>
              <div className="sl-editor__nudge" aria-label="Dời từng điểm ảnh">
                <button type="button" className="sl-iconbtn sl-iconbtn--44" onClick={() => nudge(-1, 0)} aria-label="Dời sang trái">
                  <Icon name="back" size={18} />
                </button>
                <button type="button" className="sl-iconbtn sl-iconbtn--44" onClick={() => nudge(0, -1)} aria-label="Dời lên">
                  <Icon name="back" size={18} style={{ transform: 'rotate(90deg)' }} />
                </button>
                <button type="button" className="sl-iconbtn sl-iconbtn--44" onClick={() => nudge(0, 1)} aria-label="Dời xuống">
                  <Icon name="back" size={18} style={{ transform: 'rotate(-90deg)' }} />
                </button>
                <button type="button" className="sl-iconbtn sl-iconbtn--44" onClick={() => nudge(1, 0)} aria-label="Dời sang phải">
                  <Icon name="back" size={18} style={{ transform: 'rotate(180deg)' }} />
                </button>
              </div>
              {selProblem ? (
                <span className="sl-note sl-note--err"><Icon name="alert" size={16} /><span>{selProblem}</span></span>
              ) : tooSmall ? (
                <span className="sl-note sl-note--warn">
                  <Icon name="alert" size={16} />
                  <span>
                    Ô nhỏ hơn {meta.min.w} × {meta.min.h}. Hộp xoá đúng ô này trước khi vẽ lại, nên ô hẹp
                    hơn sẽ để lại chữ cũ dính trên màn.
                  </span>
                </span>
              ) : null}
            </div>

            {sel.type === 'battery_icon' && (
              <div className="sl-note">
                <Icon name="alert" size={16} />
                <span>
                  Biểu tượng pin không đổi được màu: firmware vẽ nó bằng một ảnh nhiều màu có sẵn,
                  và mức pin đang viết cứng chứ chưa đọc pin thật. Chỉ đổi được vị trí.
                </span>
              </div>
            )}

            {VLW_KEY[sel.type] && (
              <div className="sl-field">
                <span className="sl-label">Phông chữ</span>
                <div className="sl-seg" style={{ flexWrap: 'wrap' }}>
                  {FONT_FAMILIES.map((f) => (
                    <button key={f.family} type="button" className="sl-seg__cell"
                      aria-pressed={isVlw(sel) && sel.family === f.family}
                      onClick={() => patch({ font: VLW_KEY[sel.type], family: f.family })}
                      style={{ flexBasis: '30%', fontSize: 12, fontFamily: `"${f.family}"` }}>
                      {f.label}
                    </button>
                  ))}
                  <button type="button" className="sl-seg__cell"
                    aria-pressed={!isVlw(sel)}
                    onClick={() => patch({ font: BUILTIN_FONTS[sel.type].value })}
                    style={{ flexBasis: '60%', fontSize: 12 }}>
                    {BUILTIN_FONTS[sel.type].label}
                  </button>
                </div>
                {isVlw(sel) ? (
                  <>
                    <span style={{ display: 'flex', justifyContent: 'space-between' }}>
                      <span className="sl-caption">Cỡ chữ</span>
                      <span className="sl-label-s">{sel.px}px</span>
                    </span>
                    <input type="range" className="sl-range" min={PX_RANGE[sel.type][0]} max={PX_RANGE[sel.type][1]}
                      step={1} value={sel.px} onChange={(e) => patch({ px: Number(e.target.value) })} />
                  </>
                ) : (
                  <span className="sl-caption" style={{ color: 'var(--neutral-400)' }}>
                    Phông nằm sẵn trong hộp, chỉ có chữ Latin không dấu.
                  </span>
                )}
              </div>
            )}

            {sel.type === 'clock_date' && (
              <div style={{ display: 'flex', gap: 10 }}>
                <label className="sl-field" style={{ flex: 2 }}>
                  <span className="sl-label">Dạng ngày</span>
                  <select className="sl-input" value={sel.format} onChange={(e) => patch({ format: e.target.value })}>
                    {DATE_FORMATS.map((f) => <option key={f.value} value={f.value}>{f.label}</option>)}
                  </select>
                </label>
                <div className="sl-field" style={{ flex: 1 }}>
                  <span className="sl-label">Thứ</span>
                  <div className="sl-seg">
                    {[['vi', 'Việt'], ['en', 'Anh']].map(([v, l]) => (
                      <button key={v} type="button" className="sl-seg__cell" aria-pressed={sel.locale === v}
                        onClick={() => patch({ locale: v })}>{l}</button>
                    ))}
                  </div>
                </div>
              </div>
            )}

            {(meta.color || meta.align) && (
              <div style={{ display: 'flex', gap: 10 }}>
                {meta.color && (
                  <label className="sl-field" style={{ flex: 1 }}>
                    <span className="sl-label">Màu</span>
                    <span className="sl-input" style={{ display: 'flex', alignItems: 'center', gap: 'var(--sp-2)', padding: '6px 10px' }}>
                      <input
                        type="color" aria-label="Chọn màu"
                        value={/^#[0-9A-Fa-f]{6}$/.test(sel.color || '') ? sel.color : '#000000'}
                        onChange={(e) => patch({ color: e.target.value.toUpperCase() })}
                        style={{ width: 24, height: 24, padding: 0, border: 'none', background: 'none', flex: '0 0 auto', cursor: 'pointer' }}
                      />
                      <input
                        value={sel.color || ''} maxLength={7} spellCheck={false} aria-label="Mã màu"
                        onChange={(e) => patch({ color: e.target.value })}
                        style={{
                          border: 'none', outline: 'none', background: 'none', width: '100%',
                          font: 'inherit', fontWeight: 600, color: 'var(--caramel-900)',
                        }}
                      />
                    </span>
                  </label>
                )}

                {meta.align && (
                  <div className="sl-field" style={{ flex: 1 }}>
                    <span className="sl-label">Căn lề</span>
                    <div className="sl-seg">
                      {ALIGNS.map((a) => (
                        <button key={a.value} type="button" className="sl-seg__cell"
                          aria-pressed={sel.align === a.value} aria-label={a.label}
                          onClick={() => patch({ align: a.value })}>
                          <Icon name={a.icon} size={16} />
                        </button>
                      ))}
                    </div>
                  </div>
                )}
              </div>
            )}

            {badHex && (
              <div className="sl-note sl-note--err">
                <Icon name="alert" size={16} />
                <span>Viết đủ bảy ký tự. #000 không phải viết tắt của #000000 — hộp sẽ vẽ chữ màu trắng.</span>
              </div>
            )}
          </div>
        ) : (
          <span className="sl-caption" style={{ alignSelf: 'center', textAlign: 'center' }}>
            Chạm một widget trên màn để sửa, kéo để dời chỗ.
          </span>
        )}

        {/* --- background image --- */}
        <div className="sl-listcard">
          <Icon name="image" size={20} style={{ color: 'var(--chip-fg)' }} />
          <div className="sl-listcard__mid">
            <span className="sl-label-s">Ảnh nền</span>
            <span className="sl-caption">
              {bgBusy ? 'Đang xử lý ảnh…'
                : bg?.isDefault ? 'Nền mặc định của hộp'
                : bg ? 'Ảnh riêng, cắt vuông ở giữa'
                : 'Nền đen'}
            </span>
          </div>
          {!bg && !bgBusy && (
            <button type="button" className="sl-btn sl-btn--sec" onClick={() => changeBg(() => loadDefaultBackground(DEFAULT_BG_URL))}
              style={{ minHeight: 36, padding: '0 var(--sp-3)' }}>
              Nền mặc định
            </button>
          )}
          {bg && (
            <button type="button" className="sl-iconbtn" onClick={() => { setBg(null); setTouched(true); }} aria-label="Bỏ ảnh nền">
              <Icon name="trash" size={18} />
            </button>
          )}
          <label className="sl-btn sl-btn--sec" style={{ minHeight: 36, padding: '0 var(--sp-3)', cursor: 'pointer' }}>
            {bg ? 'Đổi' : 'Chọn ảnh'}
            <input type="file" accept="image/*" onChange={pickBackground} disabled={bgBusy} style={{ display: 'none' }} />
          </label>
        </div>
        {bgError && <div className="sl-reason">{bgError}</div>}

        {fontIssue && (
          <div className="sl-note sl-note--warn">
            <Icon name="alert" size={16} />
            <span>{fontIssue}</span>
          </div>
        )}

        {problems.length > 0 && (
          <div className="sl-note sl-note--err">
            <Icon name="alert" size={16} />
            <span>Còn {problems.length} widget chưa hợp lệ — sửa xong mới gửi được.</span>
          </div>
        )}

        <Actions>
          <Button kind="pri" disabled={problems.length > 0 || widgets.length === 0 || bgBusy} onClick={goSend}>
            Xong — xem phần sẽ gửi
          </Button>
        </Actions>
      </Body>

      {adding && (
        <Modal onClose={() => setAdding(false)}>
          <span className="sl-heading">Thêm widget</span>
          {Object.entries(WIDGET_TYPES).map(([type, m]) => (
            <button key={type} type="button" className="sl-listcard sl-msgrow" onClick={() => addWidget(type)}>
              <Icon name={m.icon} size={20} style={{ color: 'var(--chip-fg)' }} />
              <span className="sl-listcard__mid">
                <span className="sl-label-s">{m.label}</span>
                <span className="sl-caption">
                  {type === 'battery_icon' ? 'Ảnh cố định, mức pin chưa đọc pin thật' : `Tối thiểu ${m.min.w} × ${m.min.h}`}
                </span>
              </span>
              <Icon name="plus" size={16} style={{ color: 'var(--neutral-400)' }} />
            </button>
          ))}
          <Button kind="gho" onClick={() => setAdding(false)}>Đóng</Button>
        </Modal>
      )}

      {confirmLeave && (
        <Modal onClose={() => setConfirmLeave(false)}>
          <span className="sl-heading">Bỏ các thay đổi?</span>
          <span className="sl-body">Giao diện chưa được lưu. Quay lại bây giờ thì các chỉnh sửa vừa rồi sẽ mất.</span>
          <Button kind="pri" onClick={() => setConfirmLeave(false)}>Sửa tiếp</Button>
          <Button kind="gho" onClick={goBack}>Bỏ thay đổi</Button>
        </Modal>
      )}
    </Screen>
  );
}
