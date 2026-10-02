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
 * Alarm list + alarm editor dialog. Two firmware rules the UI must show:
 *   MAX_ALARMS = 10 — when full, the add button is disabled.
 *   repeatable = false is a one-shot alarm: after ringing it stays in the list,
 *   switched off — the copy must say so.
 */

const MAX_ALARMS = 10;
// Alarm volume: default 80 (matches firmware + backend). Below 20 may be
// inaudible while asleep; to silence an alarm, turn it off instead.
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
      // The music library is only for showing track names; alarms still work if it fails to load.
      listMusic(boxId).then((r) => r.success && setMusic(r.data || [])).catch(() => {});
      const res = await getAlarms(boxId);
      // RTDB returns key order (alarm_<creation time>), not ring-time order.
      // 24h "HH:mm" strings sort chronologically.
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

  /* Toggle from the list: update optimistically for instant feedback; on
     failure reload from the server instead of guessing the previous state. */
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
        music_id: editing.music_id || '', // "" = beep
        volume: editing.volume,
        ramp: editing.ramp,
      };
      if (editing.id) {
        // Changing the time of a disabled alarm means "ring at the new time" → enable it.
        // Editing only music / volume keeps the on-off state.
        const wake = editing.time !== editing.origTime && !editing.origEnabled;
        await updateAlarm(boxId, editing.id, wake ? { ...fields, is_enable: true } : fields);
      } else {
        await createAlarm(boxId, { ...fields, is_enable: true });
      }
      setEditing(null);
      // a_flag: the box re-reads the list only on its next wake-up.
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

        {/* Usage counter — switches to a warning tone at the cap of 10 */}
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

        {/* How to stop a ringing box — matches the firmware (config.h ALARM_SNOOZE_SEC /
            ALARM_RING_MAX_MS). Shown once here instead of in every edit popup. */}
        <div className="sl-howto">
          <Icon name="tap" size={22} />
          <span><b>Chạm</b> để báo lại sau 5 phút · <b>giữ 3 giây</b> để tắt · tự tắt sau 1 phút</span>
        </div>
      </Body>

      {toast}

      {/* Delete confirmation is a STEP inside the same modal, not a stacked one: each
          Modal registers its own Esc listener, so two stacked modals would close together. */}
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

          {/* The browser's time input yields 24h "HH:mm" — the format
              validation.middleware.ts requires */}
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
            {/* Music reaches the box only when an alarm uses it; if it hasn't downloaded by ring time, the box beeps. */}
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
