import React, { useState } from 'react';
import { useParams, useNavigate } from 'react-router-dom';
import Icon from '../components/ui/Icon';
import { Screen, AppBar, Body, Header, Button, Modal } from '../components/ui/Screen';
import BoxScreen from '../components/theme/BoxScreen';
import { DEFAULT_WIDGETS, FONTS, ALIGNS, MIN_SIZE, SCREEN } from '../theme/layout';

/**
 * Màn 20 "theme editor".
 *
 * Mỗi ô trong bảng thuộc tính chỉ có mặt nếu LayoutEngine.cpp THẬT SỰ đọc
 * trường đó:
 *   font    -> drawClockTime/drawClockDate, if-chain 3 nhánh cứng
 *   color   -> hexToColor, bắt buộc đúng 7 ký tự #RRGGBB
 *   align   -> drawTextWidget: center / right / còn lại = trái
 *   x,y,w,h -> drawBackgroundPatch xoá đúng ô w×h trước khi vẽ
 *
 * CỐ Ý KHÔNG CÓ: "format" (đọc vào cfg.format rồi không ai dùng), "src", widget
 * ảnh (case WIDGET_IMAGE: break;), và ô chọn màu cho pin (drawBatteryIcon bỏ
 * qua cfg.color). Bày ra một control mà firmware bỏ qua là hứa suông.
 */
export default function ThemeEditor() {
  const { boxId } = useParams();
  const navigate = useNavigate();
  const [widgets, setWidgets] = useState(DEFAULT_WIDGETS);
  const [selectedId, setSelectedId] = useState('clock_time');

  const sel = widgets.find((w) => w.id === selectedId);
  const patch = (fields) =>
    setWidgets((prev) => prev.map((w) => (w.id === selectedId ? { ...w, ...fields } : w)));

  const min = sel ? MIN_SIZE[sel.type] : null;
  const tooSmall = sel && min && !sel.fixed && (sel.w < min.w || sel.h < min.h);
  /* hexToColor: strlen(hex) < 7 || hex[0] != '#' -> trả TFT_WHITE, im lặng. */
  const badHex = sel && !sel.fixed && !/^#[0-9A-Fa-f]{6}$/.test(sel.color || '');

  return (
    <Screen>
      <AppBar onBack={() => navigate(`/box/${boxId}/receiver/theme`)} />
      <Body>
        <Header title="Sửa giao diện" to="Mặc định" />

        <div className="sl-card" style={{ padding: 10, gap: 10 }}>
          <BoxScreen widgets={widgets} selectedId={selectedId} onSelect={setSelectedId} showBoxes />
          {sel && !sel.fixed && (
            <span className="sl-caption" style={{ alignSelf: 'center', fontWeight: 600, color: 'var(--rose-700)' }}>
              {sel.w} × {sel.h} tại {sel.x}, {sel.y}
            </span>
          )}
        </div>

        <div style={{ display: 'flex', alignItems: 'center', gap: 'var(--sp-2)' }}>
          <Icon name="layers" size={16} style={{ color: 'var(--neutral-500)' }} />
          <span className="sl-label" style={{ flex: 1 }}>Widget</span>
          <span className="sl-caption" style={{ fontWeight: 500 }}>{widgets.length}</span>
        </div>

        <div style={{ display: 'flex', flexDirection: 'column', gap: 'var(--sp-2)' }}>
          {widgets.map((w) => {
            const on = w.id === selectedId;
            return (
              <button
                key={w.id} type="button" className="sl-listcard" onClick={() => setSelectedId(w.id)}
                style={{
                  cursor: 'pointer',
                  background: on ? 'var(--rose-50)' : 'var(--neutral-0)',
                  borderColor: on ? 'var(--rose-400)' : 'var(--caramel-300)',
                  borderWidth: on ? 1 : 0.5,
                }}
              >
                <Icon name={w.icon} size={20} style={{ color: on ? 'var(--rose-700)' : 'var(--caramel-700)' }} />
                <div className="sl-listcard__mid">
                  <span className="sl-label-s">{w.label}</span>
                  <span className="sl-caption">
                    {w.fixed ? `Biểu tượng cố định · ${w.w} × ${w.h}` : `${w.w} × ${w.h} tại ${w.x}, ${w.y}`}
                  </span>
                </div>
                <Icon name="move" size={16} style={{ color: 'var(--neutral-400)' }} />
              </button>
            );
          })}
        </div>

        <button type="button" className="sl-addrow" disabled>
          <Icon name="plus" size={20} />
          Thêm widget — cần firmware mới
        </button>
      </Body>

      {sel && (
        <Modal>
          <div style={{ display: 'flex', alignItems: 'center', gap: 'var(--sp-2)' }}>
            <Icon name={sel.icon} size={20} style={{ color: 'var(--rose-700)' }} />
            <span className="sl-heading" style={{ flex: 1 }}>{sel.label}</span>
            <span style={{
              padding: '3px 8px', borderRadius: 999, background: 'var(--caramel-100)',
              fontSize: 12, fontWeight: 500, color: 'var(--caramel-800)',
            }}>{sel.type}</span>
            {/* Không có nút này thì bảng thuộc tính mở vĩnh viễn và danh sách
                widget bên dưới không bao giờ chạm tới được. */}
            <button type="button" className="sl-iconbtn" onClick={() => setSelectedId(null)} aria-label="Đóng">
              <Icon name="x" size={20} />
            </button>
          </div>

          {sel.fixed ? (
            /* drawBatteryIcon: pushImage một ảnh nhiều màu, bỏ qua cfg.color; và
               mức pin là `int state = 3;` viết cứng. Không có gì để sửa. */
            <div className="sl-note">
              <Icon name="alert" size={16} />
              <span>
                Biểu tượng pin không đổi được màu hay kích thước: firmware vẽ nó bằng một ảnh
                nhiều màu có sẵn, và mức pin đang viết cứng chứ chưa đọc pin thật.
              </span>
            </div>
          ) : (
            <>
              <div className="sl-field">
                <span className="sl-label">Phông chữ</span>
                <div className="sl-seg" style={{ flexWrap: 'wrap' }}>
                  {FONTS.map((f) => (
                    <button key={f.value} type="button" className="sl-seg__cell"
                      aria-pressed={sel.font === f.value}
                      onClick={() => patch({ font: f.value })}
                      style={{ flexBasis: '30%', fontSize: 12 }}>
                      {f.label}
                    </button>
                  ))}
                </div>
                <span className="sl-caption" style={{ color: 'var(--neutral-400)' }}>
                  Ba phông này nằm trong firmware. Tên khác sẽ âm thầm rơi về Chakra Petch 48.
                </span>
              </div>

              <div style={{ display: 'flex', gap: 10 }}>
                <label className="sl-field" style={{ flex: 1 }}>
                  <span className="sl-label">Màu</span>
                  <span className="sl-input" style={{ display: 'flex', alignItems: 'center', gap: 'var(--sp-2)', padding: '9px 10px' }}>
                    <span style={{
                      width: 18, height: 18, borderRadius: '50%', flex: '0 0 auto',
                      background: /^#[0-9A-Fa-f]{6}$/.test(sel.color) ? sel.color : '#FFFFFF',
                      border: '0.5px solid var(--neutral-100)',
                    }} />
                    <input
                      value={sel.color} maxLength={7} spellCheck={false}
                      onChange={(e) => patch({ color: e.target.value })}
                      style={{
                        border: 'none', outline: 'none', background: 'none', width: '100%',
                        font: 'inherit', fontWeight: 600, color: 'var(--caramel-900)',
                      }}
                    />
                  </span>
                </label>

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
              </div>

              {badHex ? (
                <div className="sl-note sl-note--err">
                  <Icon name="alert" size={16} />
                  <span>
                    Viết đủ bảy ký tự. #000 không phải viết tắt của #000000 — hộp sẽ vẽ chữ màu trắng.
                  </span>
                </div>
              ) : (
                <span className="sl-caption" style={{ color: 'var(--neutral-400)' }}>
                  Viết đủ bảy ký tự dạng #RRGGBB.
                </span>
              )}

              <div className="sl-field">
                <span className="sl-label">Ô trên màn hình</span>
                <div style={{ display: 'flex', gap: 'var(--sp-2)' }}>
                  {['x', 'y', 'w', 'h'].map((k) => (
                    <label key={k} className="sl-input" style={{ flex: 1, display: 'flex', flexDirection: 'column', gap: 4, padding: '8px 10px' }}>
                      <span className="sl-caption" style={{ textTransform: 'uppercase' }}>{k}</span>
                      <input
                        type="number" min={0} max={SCREEN} value={sel[k]}
                        onChange={(e) => patch({ [k]: Number(e.target.value) })}
                        style={{
                          border: 'none', outline: 'none', background: 'none', width: '100%',
                          font: 'inherit', fontSize: 15, fontWeight: 600, color: 'var(--caramel-900)',
                        }}
                      />
                    </label>
                  ))}
                </div>
                {/* Ghi chú xám khi giá trị hợp lệ, dải đỏ khi thật sự vi phạm:
                    cảnh báo trên một giá trị đúng thì lần sau người dùng bỏ qua
                    cả cảnh báo thật. */}
                {tooSmall ? (
                  <span className="sl-note sl-note--err">
                    <Icon name="alert" size={16} />
                    <span>
                      Ô nhỏ hơn {min.w} × {min.h}. Hộp xoá đúng ô này trước khi vẽ lại, nên ô hẹp
                      hơn sẽ để lại chữ cũ dính trên màn.
                    </span>
                  </span>
                ) : (
                  <span className="sl-caption" style={{ color: 'var(--neutral-400)' }}>
                    Tối thiểu {min.w} × {min.h}. Hộp xoá đúng ô này trước khi vẽ lại.
                  </span>
                )}
              </div>
            </>
          )}

          <Button kind="pri" onClick={() => navigate(`/box/${boxId}/receiver/theme/send`)}>
            Xong — xem phần sẽ gửi
          </Button>
        </Modal>
      )}
    </Screen>
  );
}
