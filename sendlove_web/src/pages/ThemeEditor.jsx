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
 * Màn 20 "theme editor".
 *
 * Mỗi ô trong bảng thuộc tính chỉ có mặt nếu LayoutEngine.cpp THẬT SỰ đọc
 * trường đó cho loại widget này (WIDGET_TYPES trong theme/layout.js):
 *   font    -> họ phông web (cắt thành VLW khi gửi) hoặc phông có sẵn trong firmware
 *   format  -> clock_date: LayoutEngine::formatDate (+ locale vi/en)
 *   color   -> hexToColor, bắt buộc đúng 7 ký tự #RRGGBB
 *   align   -> drawTextWidget: center / right / còn lại = trái
 *   x,y,w,h -> drawBackgroundPatch xoá đúng ô w×h trước khi vẽ
 *
 * CỐ Ý KHÔNG CÓ: widget ảnh (firmware không vẽ), màu cho pin (drawBatteryIcon bỏ qua
 * cfg.color).
 *
 * Bản nháp nhận từ ThemePicker qua location.state; mở thẳng URL thì tự đọc
 * bản đã lưu, không có thì dùng bố cục mặc định của firmware.
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

  // Nạp các họ phông để xem trước đúng hình chữ (web cắt đúng các phông này thành VLW).
  useEffect(() => {
    FONT_FAMILIES.forEach((f) => ensureWebFont(f.family, f.weight).catch(() => {}));
  }, []);

  // Mở thẳng /theme/edit (không qua picker): nạp bản đã lưu nếu có.
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
  const patch = (fields) =>
    setWidgets((prev) => prev.map((w) => (w.id === selectedId ? { ...w, ...fields } : w)));

  const tooSmall = sel && meta && (sel.w < meta.min.w || sel.h < meta.min.h);
  const badHex = sel && meta?.color && !/^#[0-9A-Fa-f]{6}$/.test(sel.color || '');
  const problems = widgets.map(widgetProblem).filter(Boolean);

  const addWidget = (type) => {
    const w = newWidget(type, widgets);
    setWidgets((prev) => [...prev, w]);
    setAdding(false);
    setSelectedId(w.id);
  };

  const removeSelected = () => {
    setWidgets((prev) => prev.filter((w) => w.id !== selectedId));
    setSelectedId(null);
  };

  const pickBackground = async (e) => {
    const file = e.target.files[0];
    e.target.value = '';
    if (!file) return;
    setBgBusy(true);
    setBgError(null);
    try {
      setBg(await imageToRgb565(file));
    } catch {
      setBgError('Không đọc được ảnh này. Thử một ảnh JPG hoặc PNG khác.');
    } finally {
      setBgBusy(false);
    }
  };

  const useDefaultBg = async () => {
    setBgBusy(true);
    setBgError(null);
    try {
      setBg(await loadDefaultBackground(DEFAULT_BG_URL));
    } catch {
      setBgError('Không tải được nền mặc định.');
    } finally {
      setBgBusy(false);
    }
  };

  const fontIssue = sharedFontIssue(widgets);
  const bgUrl = bg?.url || bg?.previewUrl || null;
  const goSend = () => navigate(`/box/${boxId}/receiver/theme/send`, { state: { name, widgets, bg } });

  return (
    <Screen>
      <AppBar onBack={() => navigate(`/box/${boxId}/receiver/theme`)} />
      <Body>
        <Header title="Sửa giao diện" to={name} />

        <div className="sl-card" style={{ padding: 10, gap: 10 }}>
          <BoxScreen widgets={widgets} selectedId={selectedId} onSelect={setSelectedId} showBoxes background={bgUrl} />
          <span className="sl-caption" style={{ alignSelf: 'center' }}>Chạm một ô trên màn để sửa</span>
        </div>

        {/* --- ảnh nền --- */}
        <div className="sl-listcard">
          <Icon name="image" size={20} style={{ color: 'var(--chip-fg)' }} />
          <div className="sl-listcard__mid">
            <span className="sl-label-s">Ảnh nền</span>
            <span className="sl-caption">
              {bgBusy ? 'Đang xử lý ảnh…'
                : bg?.isDefault ? 'Nền mặc định của hộp · 115,2 KB'
                : bg ? 'Ảnh riêng · 240 × 240 RGB565, 115,2 KB'
                : 'Nền đen (hộp không còn nền dựng sẵn)'}
            </span>
          </div>
          {!bg && !bgBusy && (
            <button type="button" className="sl-btn sl-btn--gho" onClick={useDefaultBg}
              style={{ minHeight: 36, padding: '0 var(--sp-3)' }}>
              Nền mặc định
            </button>
          )}
          {bg && (
            <button type="button" className="sl-iconbtn" onClick={() => setBg(null)} aria-label="Bỏ ảnh nền">
              <Icon name="trash" size={18} />
            </button>
          )}
          <label className="sl-btn sl-btn--gho" style={{ minHeight: 36, padding: '0 var(--sp-3)', cursor: 'pointer' }}>
            {bg ? 'Đổi' : 'Chọn ảnh'}
            <input type="file" accept="image/*" onChange={pickBackground} disabled={bgBusy} style={{ display: 'none' }} />
          </label>
        </div>
        {bgError && <div className="sl-reason">{bgError}</div>}
        {bg?.previewUrl && (
          <span className="sl-caption" style={{ color: 'var(--neutral-400)', marginTop: -8 }}>
            Ảnh được cắt vuông ở giữa. Màn hộp chỉ có 65 nghìn màu nên dải màu mịn sẽ hơi bậc thang — xem trước ở trên đã tính cả điều đó.
          </span>
        )}

        {/* --- danh sách widget --- */}
        <div style={{ display: 'flex', alignItems: 'center', gap: 'var(--sp-2)' }}>
          <Icon name="layers" size={16} style={{ color: 'var(--neutral-500)' }} />
          <span className="sl-label" style={{ flex: 1 }}>Widget</span>
          <span className="sl-caption" style={{ fontWeight: 500 }}>{widgets.length}/{MAX_WIDGETS}</span>
        </div>

        <div style={{ display: 'flex', flexDirection: 'column', gap: 'var(--sp-2)' }}>
          {widgets.map((w) => {
            const bad = widgetProblem(w);
            return (
              <button
                key={w.id} type="button" className="sl-listcard sl-msgrow" onClick={() => setSelectedId(w.id)}
                style={bad ? { borderColor: 'var(--error-fill)' } : undefined}
              >
                <Icon name={w.icon} size={20} style={{ color: 'var(--caramel-700)' }} />
                <span className="sl-listcard__mid">
                  <span className="sl-label-s">{w.label}</span>
                  <span className="sl-caption" style={bad ? { color: 'var(--error-text)' } : undefined}>
                    {bad || `${w.w} × ${w.h} tại ${w.x}, ${w.y}`}
                  </span>
                </span>
                <Icon name="chevron" size={16} style={{ color: 'var(--neutral-400)' }} />
              </button>
            );
          })}
          {widgets.length === 0 && (
            <span className="sl-caption">Chưa có widget nào — màn hộp sẽ chỉ có ảnh nền.</span>
          )}
        </div>

        <button type="button" className="sl-addrow" disabled={widgets.length >= MAX_WIDGETS} onClick={() => setAdding(true)}>
          <Icon name="plus" size={20} />
          {widgets.length >= MAX_WIDGETS ? `Tối đa ${MAX_WIDGETS} widget` : 'Thêm widget'}
        </button>

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

      {sel && meta && (
        <Modal onClose={() => setSelectedId(null)}>
          <div style={{ display: 'flex', alignItems: 'center', gap: 'var(--sp-2)' }}>
            <Icon name={sel.icon} size={20} style={{ color: 'var(--text-accent)' }} />
            <span className="sl-heading" style={{ flex: 1 }}>{sel.label}</span>
            <span style={{
              padding: '3px 8px', borderRadius: 999, background: 'var(--caramel-100)',
              fontSize: 12, fontWeight: 500, color: 'var(--caramel-800)',
            }}>{sel.type}</span>
            <button type="button" className="sl-iconbtn" onClick={() => setSelectedId(null)} aria-label="Đóng">
              <Icon name="x" size={20} />
            </button>
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
                  <span className="sl-caption" style={{ color: 'var(--neutral-400)' }}>
                    Web cắt đúng các ký tự cần dùng thành phông cho hộp, có dấu tiếng Việt.
                  </span>
                </>
              ) : (
                <span className="sl-caption" style={{ color: 'var(--neutral-400)' }}>
                  Phông nằm sẵn trong hộp, không cần tải thêm. Chỉ có chữ Latin không dấu.
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

          <div className="sl-field">
            <span className="sl-label">Ô trên màn hình</span>
            <div style={{ display: 'flex', gap: 'var(--sp-2)' }}>
              {['x', 'y', 'w', 'h'].map((k) => (
                <label key={k} className="sl-input" style={{ flex: 1, display: 'flex', flexDirection: 'column', gap: 4, padding: '8px 10px' }}>
                  <span className="sl-caption" style={{ textTransform: 'uppercase' }}>{k}</span>
                  <input
                    type="number" min={0} max={SCREEN} step={1} value={sel[k]}
                    disabled={meta.fixedSize && (k === 'w' || k === 'h')}
                    onChange={(e) => patch({ [k]: Math.round(Number(e.target.value)) || 0 })}
                    style={{
                      border: 'none', outline: 'none', background: 'none', width: '100%',
                      font: 'inherit', fontSize: 15, fontWeight: 600, color: 'var(--caramel-900)',
                    }}
                  />
                </label>
              ))}
            </div>
            {widgetProblem(sel) ? (
              <span className="sl-note sl-note--err">
                <Icon name="alert" size={16} />
                <span>{widgetProblem(sel)}</span>
              </span>
            ) : tooSmall ? (
              <span className="sl-note sl-note--warn">
                <Icon name="alert" size={16} />
                <span>
                  Ô nhỏ hơn {meta.min.w} × {meta.min.h}. Hộp xoá đúng ô này trước khi vẽ lại, nên ô hẹp
                  hơn sẽ để lại chữ cũ dính trên màn.
                </span>
              </span>
            ) : (
              <span className="sl-caption" style={{ color: 'var(--neutral-400)' }}>
                Tối thiểu {meta.min.w} × {meta.min.h}. Hộp xoá đúng ô này trước khi vẽ lại.
              </span>
            )}
          </div>

          <Button kind="pri" onClick={() => setSelectedId(null)}>Xong</Button>
          <Button kind="gho" onClick={removeSelected} style={{ color: 'var(--error-text)' }}>
            Xoá widget này
          </Button>
        </Modal>
      )}
    </Screen>
  );
}
