import React, { useState } from 'react';
import { useParams, useNavigate } from 'react-router-dom';
import { useAuth } from '../context/AuthContext';
import Icon from '../components/ui/Icon';
import { Screen, AppBar, Body, Actions, Header, Button, Tips } from '../components/ui/Screen';
import BoxScreen, { Thumb } from '../components/theme/BoxScreen';
import { DEFAULT_WIDGETS, THEMES, NEEDS_FW } from '../theme/layout';

/**
 * Màn 19 "theme picker".
 *
 * ĐÂY LÀ HỢP ĐỒNG ĐỀ XUẤT, KHÔNG PHẢI HÀNH VI HIỆN CÓ — xem theme/layout.js.
 * Không gọi API nào, vì chưa có API nào để gọi.
 */
export default function ThemePicker() {
  const { boxId } = useParams();
  const navigate = useNavigate();
  const { profile } = useAuth();
  const [applied, setApplied] = useState('default');

  return (
    <Screen>
      <AppBar onBack={() => navigate(`/box/${boxId}/receiver/config`)} />
      <Body>
        <Header title="Giao diện màn hình hộp" to={profile?.boxes_list?.[boxId]?.box_name || `Hộp ${boxId}`} />

        <div className="sl-note sl-note--warn">
          <Icon name="alert" size={16} />
          <span>
            Hộp của bạn chưa đọc được giao diện gửi từ đây. Cần firmware {NEEDS_FW} trở lên —
            màn này dựng trước để chốt cách làm.
          </span>
        </div>

        {/* --- xem trước đúng kích thước thật ---
            Chỉ có bố cục của giao diện mặc định là đọc được từ firmware
            (defaultLayoutJson, main.cpp:54-61). Hai giao diện kia chưa tồn tại,
            nên nhãn nói đúng là "mặc định" chứ không đổi theo lựa chọn — đổi
            nhãn mà không đổi hình là màn hình nói dối. */}
        <div className="sl-card" style={{ padding: 'var(--sp-3)', gap: 10 }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: 'var(--sp-2)' }}>
            <Icon name="palette" size={16} style={{ color: 'var(--rose-700)' }} />
            <span className="sl-caption" style={{ flex: 1, fontWeight: 500 }}>Đang trên hộp</span>
            <span className="sl-caption" style={{ fontWeight: 600, color: 'var(--caramel-900)' }}>
              Mặc định
            </span>
          </div>
          <BoxScreen widgets={DEFAULT_WIDGETS} />
          {applied !== 'default' && (
            <span className="sl-caption" style={{ color: 'var(--neutral-400)', textAlign: 'center' }}>
              Xem trước của “{THEMES.find((t) => t.id === applied)?.name}” cần bố cục thật
              từ hộp — chưa có, nên đây vẫn là giao diện đang chạy.
            </span>
          )}
        </div>

        {/* --- danh sách giao diện --- */}
        <div style={{ display: 'flex', alignItems: 'center', gap: 'var(--sp-2)' }}>
          <span className="sl-label" style={{ flex: 1 }}>Giao diện</span>
          <span className="sl-caption" style={{ fontWeight: 500 }}>{THEMES.length}</span>
        </div>

        <div style={{ display: 'flex', flexDirection: 'column', gap: 'var(--sp-2)' }}>
          {THEMES.map((t) => {
            const on = t.id === applied;
            return (
              <button
                key={t.id} type="button" className="sl-listcard" onClick={() => setApplied(t.id)}
                style={{
                  padding: '10px var(--sp-4)', cursor: 'pointer',
                  borderColor: on ? 'var(--rose-400)' : 'var(--caramel-300)',
                  borderWidth: on ? 1 : 0.5,
                }}
              >
                <Thumb bars={t.bars} />
                <div className="sl-listcard__mid">
                  <span className="sl-label-s" style={{ fontSize: 15 }}>{t.name}</span>
                  <span className="sl-caption">{t.what}</span>
                </div>
                {on
                  ? <Icon name="check" size={20} style={{ color: 'var(--rose-700)' }} />
                  : <Icon name="chevron" size={16} style={{ color: 'var(--neutral-400)' }} />}
              </button>
            );
          })}
        </div>

        {/* LayoutEngine.cpp có `case WIDGET_IMAGE: break;` — widget ảnh phân tích
            được nhưng không vẽ gì; nền là mảng biên dịch sẵn, không đọc từ thẻ. */}
        <Tips>
          Ảnh và hình nền cần bản firmware mới. Giao diện gửi hôm nay chỉ đổi được
          bố cục, phông chữ và màu.
        </Tips>

        <Actions>
          <Button kind="pri" onClick={() => navigate(`/box/${boxId}/receiver/theme/edit`)}>
            Sửa giao diện này
          </Button>
        </Actions>
      </Body>
    </Screen>
  );
}
