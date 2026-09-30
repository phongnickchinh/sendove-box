import React, { useCallback, useEffect, useState } from 'react';
import { useParams, useNavigate } from 'react-router-dom';
import { getAlarms, createAlarm, updateAlarm, deleteAlarm } from '../api/alarm';
import { listMusic } from '../api/music';
import { useAuth } from '../context/AuthContext';
import Icon from '../components/ui/Icon';
import { Screen, AppBar, Body, Header, Button, Modal, Tips } from '../components/ui/Screen';
import Illustration from '../components/ui/Illustration';
import { useToast } from '../components/ui/Toast';

/**
 * Màn 10 "alarm config dialog" + màn 12 "alarm config full".
 *
 * Hai luật của firmware phải hiện thẳng trên UI, không giấu:
 *   MAX_ALARMS = 10 (config.h:74) — đầy thì nút thêm phải vô hiệu, không phải
 *     để người dùng bấm rồi nhận 400.
 *   repeatable = false KHÔNG phải "không lặp lại" mà là báo thức một lần: kêu
 *     xong firmware tự set is_enable = false. Hàng vẫn nằm trong danh sách ở
 *     trạng thái tắt — nên copy phải nói rõ chứ không thì người dùng tưởng hỏng.
 */

const MAX_ALARMS = 10;
// Âm lượng báo thức: mặc định 80 (khớp firmware + backend). Dưới 20 có thể không nghe
// thấy lúc đang ngủ; muốn im thì tắt báo thức.
const DEFAULT_VOLUME = 80;
const MIN_VOLUME = 20;
const NEW_ALARM = { time: '07:30', repeatable: true, music_id: '', volume: DEFAULT_VOLUME, ramp: true };

export default function ReceiverAlarms() {
  const { boxId } = useParams();
  const navigate = useNavigate();
  const { profile } = useAuth();

  const [alarms, setAlarms] = useState([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState(null);
  const [editing, setEditing] = useState(null); // null | { id?, time, repeatable, music_id, volume, ramp }
  const [saving, setSaving] = useState(false);
  const [music, setMusic] = useState([]);
  const [toast, showToast] = useToast();

  const load = useCallback(async () => {
    try {
      // Thư viện nhạc chỉ để hiện tên bài; đọc hỏng thì báo thức vẫn dùng được.
      listMusic(boxId).then((r) => r.success && setMusic(r.data || [])).catch(() => {});
      const res = await getAlarms(boxId);
      // RTDB trả theo thứ tự key (alarm_<thời điểm tạo>), không theo giờ kêu.
      // "HH:mm" 24h so sánh chuỗi là đúng thứ tự thời gian.
      if (res.success) setAlarms((res.data || []).slice().sort((a, b) => a.time.localeCompare(b.time)));
      setError(null);
    } catch {
      setError('Không đọc được danh sách báo thức.');
    } finally {
      setLoading(false);
    }
  }, [boxId]);

  useEffect(() => { load(); }, [load]);

  const full = alarms.length >= MAX_ALARMS;

  /* Bật/tắt ngay trên danh sách: đổi trước cho tay bấm thấy phản hồi, hỏng thì
     tải lại từ máy chủ chứ không đoán trạng thái cũ. */
  const toggle = async (alarm) => {
    setAlarms((prev) => prev.map((a) => (a.id === alarm.id ? { ...a, is_enable: !a.is_enable } : a)));
    try {
      await updateAlarm(boxId, alarm.id, { is_enable: !alarm.is_enable });
    } catch {
      showToast('Không lưu được. Đang tải lại danh sách.', 'err');
      load();
    }
  };

  const save = async () => {
    setSaving(true);
    try {
      const fields = {
        time: editing.time,
        repeatable: editing.repeatable,
        music_id: editing.music_id || '', // "" = tiếng bíp
        volume: editing.volume,
        ramp: editing.ramp,
      };
      if (editing.id) {
        // Đổi giờ một báo thức đang tắt = muốn nó kêu vào giờ mới → bật luôn.
        // Chỉ sửa nhạc / âm lượng thì giữ nguyên trạng thái bật-tắt.
        const wake = editing.time !== editing.origTime && !editing.origEnabled;
        await updateAlarm(boxId, editing.id, wake ? { ...fields, is_enable: true } : fields);
      } else {
        await createAlarm(boxId, { ...fields, is_enable: true });
      }
      setEditing(null);
      // a_flag: hộp chỉ đọc lại danh sách ở lần thức dậy kế tiếp.
      showToast('Đã lưu. Hộp nhận trong vòng 5 phút.');
      await load();
    } catch (err) {
      showToast(err.response?.data?.error?.message || 'Không lưu được báo thức.', 'err');
    } finally {
      setSaving(false);
    }
  };

  const remove = async () => {
    setSaving(true);
    try {
      await deleteAlarm(boxId, editing.id);
      setEditing(null);
      await load();
    } catch {
      showToast('Không xoá được báo thức.', 'err');
    } finally {
      setSaving(false);
    }
  };

  return (
    <Screen>
      <AppBar onBack={() => navigate(`/box/${boxId}/receiver`)} />
      <Body>
        <Header title="Báo thức" to={profile?.boxes_list?.[boxId]?.box_name || `Hộp ${boxId}`} />

        {/* Đếm số đã dùng — đổi sang giọng cảnh báo khi chạm trần 10 */}
        <span style={{ display: 'flex', alignItems: 'center', gap: 6 }}>
          {full && <Icon name="alert" size={16} style={{ color: 'var(--warning-fill)' }} />}
          <span className="sl-caption" style={{ fontWeight: 500, color: full ? 'var(--warning-text)' : 'var(--neutral-500)' }}>
            Đã dùng {alarms.length}/{MAX_ALARMS} báo thức
          </span>
        </span>

        {error && <div className="sl-reason">{error}</div>}

        {loading ? (
          <span className="sl-body">Đang tải…</span>
        ) : alarms.length === 0 ? (
          <div className="sl-card sl-card--center">
            <Illustration name="alarm" />
            <span className="sl-heading">Chưa đặt báo thức nào</span>
            <span className="sl-body">Đặt một giờ, hộp sẽ sáng và kêu vào đúng lúc đó.</span>
          </div>
        ) : (
          <div style={{ display: 'flex', flexDirection: 'column', gap: 10 }}>
            {alarms.map((a) => (
              <div className="sl-listcard" key={a.id} style={{ padding: '14px var(--sp-4)' }}>
                <button
                  type="button"
                  className="sl-listcard__mid"
                  onClick={() => setEditing({
                    id: a.id, time: a.time, repeatable: a.repeatable,
                    music_id: a.music_id || '', volume: a.volume ?? DEFAULT_VOLUME, ramp: a.ramp ?? true,
                    origTime: a.time, origEnabled: a.is_enable,
                  })}
                  style={{ border: 'none', background: 'none', padding: 0, textAlign: 'left', cursor: 'pointer' }}
                >
                  <span className="sl-title" style={{ color: a.is_enable ? 'var(--caramel-900)' : 'var(--neutral-400)' }}>
                    {a.time}
                  </span>
                  <span className="sl-caption">
                    {a.repeatable
                      ? 'Mỗi ngày'
                      : a.is_enable
                        ? 'Một lần — kêu xong sẽ tự tắt'
                        : 'Một lần — đã kêu và tự tắt'}
                    {' · '}
                    {a.music_id ? (music.find((m) => m.music_id === a.music_id)?.name || 'Nhạc') : 'Tiếng bíp'}
                  </span>
                </button>
                <button
                  type="button"
                  className="sl-toggle"
                  role="switch"
                  aria-checked={a.is_enable}
                  aria-label={`${a.is_enable ? 'Tắt' : 'Bật'} báo thức ${a.time}`}
                  onClick={() => toggle(a)}
                >
                  <span className="sl-toggle__knob" />
                </button>
              </div>
            ))}
          </div>
        )}

        <button
          type="button"
          className="sl-addrow"
          disabled={full}
          onClick={() => setEditing({ ...NEW_ALARM })}
        >
          <Icon name="plus" size={20} />
          {full ? 'Danh sách báo thức đã đầy' : 'Thêm báo thức'}
        </button>

        <button
          type="button" className="sl-listcard" style={{ cursor: 'pointer' }}
          onClick={() => navigate(`/box/${boxId}/receiver/alarm/music`)}
        >
          <Icon name="bell" size={20} style={{ color: 'var(--chip-fg)' }} />
          <div className="sl-listcard__mid">
            <span className="sl-label-s">Nhạc báo thức</span>
            <span className="sl-caption">{music.length}/10 bài · thêm, đổi tên, xoá</span>
          </div>
          <Icon name="chevron" size={16} style={{ color: 'var(--neutral-400)' }} />
        </button>

        {full && <Tips>Hộp chứa được 10 báo thức. Xoá bớt một cái để có chỗ cho cái mới.</Tips>}

        {/* Cách tắt khi hộp kêu — khớp firmware (config.h ALARM_SNOOZE_SEC / ALARM_RING_MAX_MS).
            Hiện một lần ở đây thay vì lặp trong popup mỗi lần sửa. */}
        <div className="sl-howto">
          <Icon name="tap" size={22} />
          <span><b>Chạm</b> để báo lại sau 5 phút · <b>giữ 3 giây</b> để tắt · tự tắt sau 1 phút</span>
        </div>
      </Body>

      {toast}

      {/* Xác nhận xoá là một BƯỚC trong cùng modal, không phải modal chồng: mỗi Modal gắn
          một listener Esc riêng, hai modal chồng nhau sẽ đóng cùng lúc. */}
      {editing?.confirmDelete && (
        <Modal onClose={saving ? undefined : () => setEditing({ ...editing, confirmDelete: false })}>
          <span className="sl-heading">Xoá báo thức {editing.origTime}?</span>
          <span className="sl-body">Hộp sẽ không kêu vào giờ này nữa. Không hoàn tác được.</span>
          <Button kind="dan" onClick={remove} disabled={saving}>
            {saving ? 'Đang xoá…' : 'Xoá báo thức'}
          </Button>
          <Button kind="gho" onClick={() => setEditing({ ...editing, confirmDelete: false })} disabled={saving}>
            Giữ lại
          </Button>
        </Modal>
      )}

      {editing && !editing.confirmDelete && (
        <Modal onClose={saving ? undefined : () => setEditing(null)}>
          <div style={{ display: 'flex', alignItems: 'flex-start', gap: 'var(--sp-2)' }}>
            <span className="sl-heading" style={{ flex: 1 }}>{editing.id ? 'Sửa báo thức' : 'Thêm báo thức'}</span>
            <button type="button" className="sl-iconbtn" onClick={() => setEditing(null)} disabled={saving} aria-label="Đóng">
              <Icon name="x" size={20} />
            </button>
          </div>

          {/* time input của trình duyệt trả đúng "HH:mm" 24h — khớp thẳng
              pattern /^\d{2}:\d{2}$/ ở validation.middleware.ts:152 */}
          <label className="sl-field">
            <span className="sl-label">Giờ</span>
            <input
              type="time"
              className="sl-input"
              value={editing.time}
              onChange={(e) => setEditing({ ...editing, time: e.target.value })}
              style={{ fontSize: 22, fontWeight: 700, letterSpacing: 1 }}
            />
          </label>

          <div className="sl-field">
            <span className="sl-label">Tần suất</span>
            <div className="sl-seg">
              <button type="button" className="sl-seg__cell" aria-pressed={editing.repeatable}
                onClick={() => setEditing({ ...editing, repeatable: true })}>
                Mỗi ngày
              </button>
              <button type="button" className="sl-seg__cell" aria-pressed={!editing.repeatable}
                onClick={() => setEditing({ ...editing, repeatable: false })}>
                Một lần
              </button>
            </div>
          </div>

          <label className="sl-field">
            <span className="sl-label">Âm báo</span>
            <select className="sl-input" value={editing.music_id}
              onChange={(e) => setEditing({ ...editing, music_id: e.target.value })}>
              <option value="">Tiếng bíp</option>
              {music.map((m) => <option key={m.music_id} value={m.music_id}>{m.name}</option>)}
            </select>
            {/* Nhạc chỉ về hộp khi báo thức dùng nó; chưa kịp tải lúc tới giờ thì bíp. */}
            {editing.music_id && (
              <span className="sl-caption" style={{ color: 'var(--neutral-400)' }}>
                Hộp tải bài này về ở lần đồng bộ kế tiếp. Chưa tải xong lúc tới giờ thì hộp kêu tiếng bíp.
              </span>
            )}
          </label>

          <label className="sl-field">
            <span style={{ display: 'flex', justifyContent: 'space-between' }}>
              <span className="sl-label">Âm lượng báo thức</span>
              <span className="sl-label-s">{editing.volume}%</span>
            </span>
            <input type="range" className="sl-range" min={MIN_VOLUME} max={100} step={5} value={editing.volume}
              onChange={(e) => setEditing({ ...editing, volume: Number(e.target.value) })} />
          </label>

          <div className="sl-listcard" style={{ padding: '10px var(--sp-3)' }}>
            <div className="sl-listcard__mid">
              <span className="sl-label-s">Tăng dần</span>
              <span className="sl-caption">Bắt đầu nhỏ rồi to dần tới mức đã chọn trong 20 giây.</span>
            </div>
            <button type="button" className="sl-toggle" role="switch" aria-checked={editing.ramp}
              aria-label="Tăng dần âm lượng" onClick={() => setEditing({ ...editing, ramp: !editing.ramp })}>
              <span className="sl-toggle__knob" />
            </button>
          </div>

          <Button kind="pri" onClick={save} disabled={saving || !editing.time}>
            {saving ? 'Đang lưu…' : 'Lưu báo thức'}
          </Button>
          {editing.id ? (
            <Button kind="gho" onClick={() => setEditing({ ...editing, confirmDelete: true })} disabled={saving}
              style={{ color: 'var(--error-text)' }}>
              Xoá báo thức này
            </Button>
          ) : (
            <Button kind="gho" onClick={() => setEditing(null)} disabled={saving}>Huỷ</Button>
          )}
        </Modal>
      )}
    </Screen>
  );
}
