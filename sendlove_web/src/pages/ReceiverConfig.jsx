import React, { useEffect, useState } from 'react';
import { useParams, useNavigate } from 'react-router-dom';
import { getBoxDetails, updateBoxConfig, updateWifi } from '../api/box';
import { useAuth } from '../context/AuthContext';
import Icon from '../components/ui/Icon';
import UnpairConfirm from '../components/UnpairConfirm';
import { fwVersion } from '../utils/boxStatus';
import { Screen, AppBar, Body, Header, Button, CircleIcon, Modal } from '../components/ui/Screen';
import { useToast } from '../components/ui/Toast';

/**
 * Box settings page + unpair confirmation.
 *
 * Three groups, three different APIs — they can't share one Save button:
 *   Wi-Fi               PUT  /boxes/:boxId/wifi    (ssid 1..32, password ≤ 63)
 *   LED/screen/speaker  PUT  /boxes/:boxId/config  (led_state, display_brightness, playback_volume)
 *   Unpair              DELETE /boxes/:boxId/unpair
 *
 * There is NO "update firmware" button: the backend has ota_flag and OtaTask
 * but no user-facing route, so a button here would be dead.
 */

const DEFAULT_CFG = { led_state: 'OFF', display_brightness: 100, playback_volume: 100 };
const CFG_KEYS = Object.keys(DEFAULT_CFG);

/* The firmware's LEDState enum. The firmware doesn't apply this section yet
   (BREATHING has no effect implemented) — the "Coming soon" badge sits on the
   section title, not on each option. */
const LED_OPTIONS = [
  { value: 'OFF', label: 'Tắt' },
  { value: 'SOLID', label: 'Sáng đều' },
  { value: 'BLINK_FAST', label: 'Nháy nhanh' },
  { value: 'BREATHING', label: 'Thở' },
];

export default function ReceiverConfig() {
  const { boxId } = useParams();
  const navigate = useNavigate();
  const { profile } = useAuth();

  const [box, setBox] = useState(null);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState(null);
  const [toast, showToast] = useToast();

  // Wi-Fi: the web can never read the password back. Empty field = keep the
  // stored password (password not sent); an open network must be chosen explicitly via openNet.
  const [savedSsid, setSavedSsid] = useState('');
  const [ssid, setSsid] = useState('');
  const [password, setPassword] = useState('');
  const [openNet, setOpenNet] = useState(false);
  const [showPass, setShowPass] = useState(false);

  // Defaults match the firmware (config.h SETTINGS_DEFAULT_*): 100 = the level the box has always used.
  const [cfg, setCfg] = useState(DEFAULT_CFG);
  const [savedCfg, setSavedCfg] = useState(DEFAULT_CFG);
  const [saving, setSaving] = useState(false);

  const [confirmUnpair, setConfirmUnpair] = useState(false);
  const [leaveTo, setLeaveTo] = useState(null); // where the user tried to go while changes were unsaved

  useEffect(() => {
    let alive = true;
    (async () => {
      try {
        const res = await getBoxDetails(boxId);
        if (!alive || !res.success) return;
        setBox(res.data);
        const s = res.data.config?.wifi_config?.ssid || '';
        setSsid(s);
        setSavedSsid(s);
        const c = {
          led_state: res.data.config?.led_state || 'OFF',
          display_brightness: res.data.config?.display_brightness ?? 100,
          playback_volume: res.data.config?.playback_volume ?? 100,
        };
        setCfg(c);
        setSavedCfg(c);
      } catch {
        if (alive) setError('Không đọc được cài đặt của hộp.');
      } finally {
        if (alive) setLoading(false);
      }
    })();
    return () => { alive = false; };
  }, [boxId]);

  const wifiDirty = ssid.trim() !== savedSsid || password !== '' || openNet;
  const cfgDirty = CFG_KEYS.some((k) => cfg[k] !== savedCfg[k]);
  const dirty = wifiDirty || cfgDirty;

  // Closing the tab / reloading with unsaved changes. The browser/phone back
  // button can NOT be intercepted: HashRouter has no useBlocker (data routers only).
  useEffect(() => {
    if (!dirty) return undefined;
    const onUnload = (e) => { e.preventDefault(); e.returnValue = ''; };
    window.addEventListener('beforeunload', onUnload);
    return () => window.removeEventListener('beforeunload', onUnload);
  }, [dirty]);

  const goTo = (path) => (dirty ? setLeaveTo(path) : navigate(path));

  const saveAll = async () => {
    if (wifiDirty) {
      if (!ssid.trim()) { showToast('Tên Wi-Fi không được để trống.', 'err'); return false; }
      // Switching networks with an empty password: "open network" and "forgot to type it" are indistinguishable.
      if (ssid.trim() !== savedSsid && !password && !openNet) {
        showToast('Nhập mật khẩu của mạng mới, hoặc bật "Mạng không có mật khẩu".', 'err');
        return false;
      }
    }
    setSaving(true);
    try {
      if (wifiDirty) {
        const body = { ssid: ssid.trim() };
        if (openNet) body.password = '';
        else if (password) body.password = password;
        await updateWifi(boxId, body);
        setSavedSsid(ssid.trim());
        setPassword('');
        setOpenNet(false);
      }
      if (cfgDirty) {
        await updateBoxConfig(boxId, cfg);
        setSavedCfg(cfg);
        // Fetch the new config_rev so the status line switches to "waiting for the box".
        const res = await getBoxDetails(boxId);
        if (res.success) setBox(res.data);
      }
      showToast('Đã lưu. Hộp áp dụng ở lần đồng bộ kế tiếp.');
      return true;
    } catch (err) {
      showToast(err.response?.data?.error?.message || 'Không lưu được. Kiểm tra kết nối rồi thử lại.', 'err');
      return false;
    } finally {
      setSaving(false);
    }
  };

  return (
    <Screen>
      <AppBar onBack={() => goTo(`/box/${boxId}/receiver`)} />
      <Body>
        <Header title="Cài đặt hộp" to={profile?.boxes_list?.[boxId]?.box_name || `Hộp ${boxId}`} />

        {error && <div className="sl-reason">{error}</div>}

        {loading ? (
          <span className="sl-body">Đang tải…</span>
        ) : (
          <>
            {/* --- Wi-Fi --- */}
            <label className="sl-field">
              {/* The current firmware doesn't read wifi_config from the DB yet (to change
                  it now, use the setup page the box serves on its own Wi-Fi) — hence the badge. */}
              <span className="sl-label" style={{ display: 'flex', alignItems: 'center', gap: 'var(--sp-2)' }}>
                Tên Wi-Fi (SSID) <span className="sl-badge">Sắp có</span>
              </span>
              <input
                className="sl-input" value={ssid} maxLength={32} autoComplete="off"
                onChange={(e) => setSsid(e.target.value)} placeholder="Ví dụ: Nha_Duyen"
              />
              {/* The ESP32-C3 has a 2.4 GHz radio only — it can't see 5 GHz networks.
                  This is the most common failure when changing Wi-Fi. */}
              <span className="sl-caption" style={{ color: 'var(--neutral-400)' }}>Chỉ Wi-Fi 2.4 GHz</span>
            </label>

            <label className="sl-field">
              <span className="sl-label">Mật khẩu Wi-Fi</span>
              <span style={{ position: 'relative', display: 'flex', alignItems: 'center' }}>
                <input
                  className="sl-input" type={showPass ? 'text' : 'password'}
                  value={password} maxLength={63} autoComplete="new-password"
                  onChange={(e) => setPassword(e.target.value)}
                  disabled={openNet}
                  placeholder={savedSsid && ssid.trim() === savedSsid ? 'Để trống để giữ mật khẩu đã lưu' : 'Mật khẩu của mạng'}
                  style={{ paddingRight: 44 }}
                />
                <button type="button" className="sl-iconbtn" onClick={() => setShowPass(!showPass)}
                  aria-label={showPass ? 'Ẩn mật khẩu' : 'Hiện mật khẩu'}
                  style={{ position: 'absolute', right: 4 }}>
                  <Icon name="eye" size={20} />
                </button>
              </span>
            </label>

            <div className="sl-listcard" style={{ padding: '10px var(--sp-3)' }}>
              <span className="sl-label-s" style={{ flex: 1 }}>Mạng không có mật khẩu</span>
              <button type="button" className="sl-toggle" role="switch" aria-checked={openNet}
                aria-label="Mạng không có mật khẩu"
                onClick={() => { setOpenNet(!openNet); setPassword(''); }}>
                <span className="sl-toggle__knob" />
              </button>
            </div>

            {/* --- LED, screen, speaker --- */}
            {/* The firmware doesn't apply led_state yet (including "breathing") — one badge for the whole section. */}
            <span className="sl-label" style={{ display: 'flex', alignItems: 'center', gap: 'var(--sp-2)' }}>
              Đèn báo <span className="sl-badge">Sắp có</span>
            </span>
            <div className="sl-seg" style={{ flexWrap: 'wrap' }}>
              {LED_OPTIONS.map((o) => (
                <button
                  key={o.value} type="button" className="sl-seg__cell"
                  aria-pressed={cfg.led_state === o.value}
                  onClick={() => setCfg({ ...cfg, led_state: o.value })}
                  style={{ flexBasis: '40%' }}
                >
                  {o.label}
                </button>
              ))}
            </div>

            {/* The firmware clamps brightness to at least 5% (SETTINGS_MIN_BRIGHTNESS): at 0
                the screen goes black and the box looks dead. Volume 0 = mute messages;
                alarms have their own volume. */}
            <Slider label="Độ sáng màn hình" value={cfg.display_brightness} min={5}
              onChange={(v) => setCfg({ ...cfg, display_brightness: v })} />
            <Slider label="Âm lượng phát tin nhắn" value={cfg.playback_volume}
              onChange={(v) => setCfg({ ...cfg, playback_volume: v })} />

            <ApplyState config={box?.config} status={box?.status} />

            {/* --- box screen theme: saved to the account, see theme/layout.js --- */}
            <button
              type="button" className="sl-listcard" style={{ cursor: 'pointer' }}
              onClick={() => goTo(`/box/${boxId}/receiver/theme`)}
            >
              <Icon name="palette" size={20} style={{ color: 'var(--chip-fg)' }} />
              <div className="sl-listcard__mid">
                <span className="sl-label-s">Giao diện màn hình hộp</span>
                <span className="sl-caption">Bố cục, phông chữ và màu trên màn 240 × 240.</span>
              </div>
              <Icon name="chevron" size={16} style={{ color: 'var(--neutral-400)' }} />
            </button>

            <SdCard status={box?.status} />

            {/* --- firmware: read-only --- */}
            <div className="sl-listcard">
              <CircleIcon size={40} bg="var(--caramel-50)" color="var(--caramel-700)" icon="gear" iconSize={20} />
              <div className="sl-listcard__mid">
                {/* Don't claim "up to date / updates itself": the firmware doesn't read
                    ota_flag; OTA is triggered by the user on the box. */}
                <span className="sl-label-s">Firmware {fwVersion(box?.status) || '—'}</span>
              </div>
            </div>

            {/* --- danger zone --- */}
            <button
              type="button" className="sl-listcard" onClick={() => setConfirmUnpair(true)}
              style={{ background: 'var(--error-bg)', borderColor: 'var(--error-fill)', cursor: 'pointer' }}
            >
              <Icon name="unlink" size={20} style={{ color: 'var(--error-fill)' }} />
              <div className="sl-listcard__mid">
                <span className="sl-label-s" style={{ color: 'var(--error-text)' }}>Huỷ ghép đôi hộp này</span>
                <span className="sl-caption" style={{ color: 'var(--error-text)' }}>
                  Dừng nhận tin nhắn và báo thức cho hộp.
                </span>
              </div>
              <Icon name="chevron" size={16} style={{ color: 'var(--error-fill)' }} />
            </button>
          </>
        )}
        {dirty && <div className="sl-savebar-spacer" aria-hidden="true" />}
      </Body>

      {dirty && (
        <div className="sl-savebar">
          <span className="sl-savebar__text">Có thay đổi chưa lưu</span>
          <Button kind="pri" block={false} onClick={saveAll} disabled={saving}>
            {saving ? 'Đang lưu…' : 'Lưu'}
          </Button>
        </div>
      )}

      {toast}

      {leaveTo && (
        <Modal onClose={() => setLeaveTo(null)}>
          <span className="sl-heading">Bỏ các thay đổi chưa lưu?</span>
          <span className="sl-body">Wi-Fi, độ sáng hay âm lượng vừa chỉnh sẽ trở lại như cũ.</span>
          <Button kind="pri" disabled={saving}
            onClick={() => { const to = leaveTo; setLeaveTo(null); saveAll().then((ok) => ok && navigate(to)); }}>
            Lưu rồi đi tiếp
          </Button>
          <Button kind="gho" onClick={() => navigate(leaveTo)}>Bỏ thay đổi</Button>
        </Modal>
      )}

      {confirmUnpair && (
        <UnpairConfirm boxId={boxId} role="receiver" onClose={() => setConfirmUnpair(false)}
          onError={(msg) => showToast(msg, 'err')} />
      )}
    </Screen>
  );
}

/**
 * SD card + latest error log (status.sd_state / sd_free_mb / log_tail sent by the firmware).
 * With a faulty card the box keeps running: the theme lives in flash and alarms
 * beep; only messages and music can't download. The fix is to replace/format
 * the card — software does nothing more.
 */
function SdCard({ status }) {
  const [openLog, setOpenLog] = useState(false);
  if (!status?.sd_state || status.sd_state === 'none') return null;
  const ok = status.sd_state === 'ok';
  return (
    <div className="sl-listcard" style={{ flexDirection: 'column', alignItems: 'stretch', gap: 6 }}>
      <span className="sl-label-s" style={{ color: ok ? undefined : 'var(--error-text)' }}>
        {ok
          ? `Thẻ nhớ hoạt động · còn trống ${status.sd_free_mb ?? '?'} MB`
          : 'Hộp không đọc được thẻ nhớ'}
      </span>
      {!ok && (
        <span className="sl-caption">
          Hộp vẫn hiện giờ và kêu báo thức, nhưng không nhận được tin nhắn và nhạc mới. Hãy cắm lại
          hoặc thay thẻ microSD (FAT32, từ 1 GB); hộp tự nhận lại ở lần đồng bộ kế tiếp.
        </span>
      )}
      {status.log_tail && (
        <>
          <button type="button" className="sl-link" onClick={() => setOpenLog(!openLog)}
            style={{ alignSelf: 'flex-start', background: 'none', border: 0, padding: 0, cursor: 'pointer' }}>
            {openLog ? 'Ẩn nhật ký lỗi' : 'Xem nhật ký lỗi gần nhất'}
          </button>
          {openLog && (
            <pre className="sl-caption" style={{ whiteSpace: 'pre-wrap', margin: 0, maxHeight: 240, overflow: 'auto' }}>
              {status.log_tail}
            </pre>
          )}
        </>
      )}
    </div>
  );
}

/**
 * Whether the box has applied the latest save: the backend bumps
 * config.config_rev on every save, and the box copies it to status.config_rev
 * after writing NVS. A sleeping box takes up to 5 minutes to wake.
 */
function ApplyState({ config, status }) {
  const want = config?.config_rev;
  if (!want) return null;
  const done = (status?.config_rev ?? 0) >= want;
  return (
    <span className="sl-caption" style={{ color: done ? 'var(--success-text)' : 'var(--neutral-400)' }}>
      {done
        ? 'Hộp đã áp dụng độ sáng và âm lượng này.'
        : 'Đang chờ hộp áp dụng (hộp đang ngủ thì có thể mất tới 5 phút).'}
    </span>
  );
}

/** Slider, with the range from validation.middleware.ts */
function Slider({ label, value, onChange, min = 0 }) {
  return (
    <label className="sl-field">
      <span style={{ display: 'flex', justifyContent: 'space-between' }}>
        <span className="sl-label">{label}</span>
        <span className="sl-label-s">{value}%</span>
      </span>
      <input
        type="range" className="sl-range" min={min} max={100} step={5} value={value}
        onChange={(e) => onChange(Number(e.target.value))}
      />
    </label>
  );
}
