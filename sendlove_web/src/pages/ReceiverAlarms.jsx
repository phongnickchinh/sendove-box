import React, { useCallback, useEffect, useState } from 'react';
import { useParams, useNavigate } from 'react-router-dom';
import { getAlarms, createAlarm, updateAlarm, deleteAlarm } from '../api/alarm';
import { useAuth } from '../context/AuthContext';
import Icon from '../components/ui/Icon';
import { Screen, AppBar, Body, Header, Button, CircleIcon, Modal, Tips } from '../components/ui/Screen';

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

export default function ReceiverAlarms() {
  const { boxId } = useParams();
  const navigate = useNavigate();
  const { profile } = useAuth();

  const [alarms, setAlarms] = useState([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState(null);
  const [editing, setEditing] = useState(null); // null | { id?, time, repeatable }
  const [saving, setSaving] = useState(false);

  const load = useCallback(async () => {
    try {
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
      setError('Không lưu được. Đang tải lại danh sách.');
      load();
    }
  };

  const save = async () => {
    setSaving(true);
    try {
      if (editing.id) {
        await updateAlarm(boxId, editing.id, { time: editing.time, repeatable: editing.repeatable });
      } else {
        await createAlarm(boxId, { time: editing.time, is_enable: true, repeatable: editing.repeatable });
      }
      setEditing(null);
      await load();
    } catch (err) {
      setError(err.response?.data?.error?.message || 'Không lưu được báo thức.');
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
      setError('Không xoá được báo thức.');
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
            <CircleIcon size={56} bg="var(--rose-50)" color="var(--rose-400)" icon="bell" iconSize={24} />
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
                  onClick={() => setEditing({ id: a.id, time: a.time, repeatable: a.repeatable })}
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
          onClick={() => setEditing({ time: '07:30', repeatable: true })}
        >
          <Icon name="plus" size={20} />
          {full ? 'Danh sách báo thức đã đầy' : 'Thêm báo thức'}
        </button>

        {full && <Tips>Hộp chứa được 10 báo thức. Xoá bớt một cái để có chỗ cho cái mới.</Tips>}
      </Body>

      {editing && (
        <Modal onClose={saving ? undefined : () => setEditing(null)}>
          <div style={{ display: 'flex', alignItems: 'flex-start', gap: 'var(--sp-2)' }}>
            <div className="sl-listcard__mid">
              <span className="sl-heading">{editing.id ? 'Sửa báo thức' : 'Thêm báo thức'}</span>
              <span className="sl-caption">Hộp sẽ kêu vào giờ này.</span>
            </div>
            <button type="button" className="sl-iconbtn" onClick={() => setEditing(null)} aria-label="Đóng">
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

          <span className="sl-caption">
            "Một lần" kêu đúng một lần rồi tự tắt — nó vẫn nằm trong danh sách để bạn bật lại.
          </span>

          {/* Khớp hành vi firmware (config.h ALARM_SNOOZE_SEC / ALARM_RING_MAX_MS) */}
          <span className="sl-caption">
            Khi hộp kêu: chạm để báo lại sau 5 phút, giữ 3 giây để tắt. Không ai chạm thì tự tắt sau 1 phút.
          </span>

          {/* a_flag: hộp chỉ đọc lại danh sách ở lần thức dậy kế tiếp */}
          <div className="sl-note">
            <Icon name="sync" size={16} />
            <span>Cài đặt vừa lưu sẽ tới hộp ở lần thức dậy kế tiếp — trong vòng 5 phút.</span>
          </div>

          <Button kind="pri" onClick={save} disabled={saving || !editing.time}>
            {saving ? 'Đang lưu…' : 'Lưu báo thức'}
          </Button>
          {editing.id ? (
            <Button kind="gho" onClick={remove} disabled={saving} style={{ color: 'var(--error-text)' }}>
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
