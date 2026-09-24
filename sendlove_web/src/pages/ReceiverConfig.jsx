import React, { useEffect, useState } from 'react';
import { useParams, useNavigate } from 'react-router-dom';
import { getBoxDetails, updateBoxConfig, updateWifi } from '../api/box';
import { useAuth } from '../context/AuthContext';
import Icon from '../components/ui/Icon';
import UnpairConfirm from '../components/UnpairConfirm';
import { fwVersion } from '../utils/boxStatus';
import { Screen, AppBar, Body, Header, Button, CircleIcon } from '../components/ui/Screen';

/**
 * Màn 11 "box config page" + màn 14 "unpair confirm".
 *
 * Ba nhóm, ba API khác nhau — không gộp vào một nút Lưu chung được:
 *   Wi-Fi          PUT  /boxes/:boxId/wifi    (ssid 1..32, password ≤ 63)
 *   Đèn/màn/loa    PUT  /boxes/:boxId/config  (led_state, display_brightness, playback_volume)
 *   Huỷ ghép đôi   DELETE /boxes/:boxId/unpair
 *
 * KHÔNG có nút "cập nhật firmware": backend có ota_flag và OtaTask nhưng không
 * có route cho người dùng bấm. Hộp tự cài. Dựng nút ở đây là dựng nút chết.
 */

/* LEDState enum ở firmware. BREATHING có trong enum nhưng hiệu ứng được ghi rõ
   là "Phase 2 — chưa triển khai", nên phải gắn nhãn chứ không để trần. */
const LED_OPTIONS = [
  { value: 'OFF', label: 'Tắt' },
  { value: 'SOLID', label: 'Sáng đều' },
  { value: 'BLINK_FAST', label: 'Nháy nhanh' },
  { value: 'BREATHING', label: 'Thở', soon: true },
];

export default function ReceiverConfig() {
  const { boxId } = useParams();
  const navigate = useNavigate();
  const { profile } = useAuth();

  const [box, setBox] = useState(null);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState(null);
  const [saved, setSaved] = useState(null);

  const [ssid, setSsid] = useState('');
  const [password, setPassword] = useState('');
  const [showPass, setShowPass] = useState(false);
  const [savingWifi, setSavingWifi] = useState(false);

  // Mặc định khớp firmware (config.h SETTINGS_DEFAULT_*): 100 = mức hộp vẫn phát trước đây.
  const [cfg, setCfg] = useState({ led_state: 'OFF', display_brightness: 100, playback_volume: 100 });
  const [savingCfg, setSavingCfg] = useState(false);

  const [confirmUnpair, setConfirmUnpair] = useState(false);

  useEffect(() => {
    let alive = true;
    (async () => {
      try {
        const res = await getBoxDetails(boxId);
        if (!alive || !res.success) return;
        setBox(res.data);
        setSsid(res.data.config?.wifi_config?.ssid || '');
        setCfg({
          led_state: res.data.config?.led_state || 'OFF',
          display_brightness: res.data.config?.display_brightness ?? 100,
          playback_volume: res.data.config?.playback_volume ?? 100,
        });
      } catch {
        if (alive) setError('Không đọc được cài đặt của hộp.');
      } finally {
        if (alive) setLoading(false);
      }
    })();
    return () => { alive = false; };
  }, [boxId]);

  const flash = (msg) => { setSaved(msg); setTimeout(() => setSaved(null), 4000); };

  const saveWifi = async () => {
    if (!ssid.trim()) { setError('Tên Wi-Fi không được để trống.'); return; }
    setSavingWifi(true); setError(null);
    try {
      await updateWifi(boxId, { ssid: ssid.trim(), password });
      setPassword('');
      flash('Đã lưu Wi-Fi lên tài khoản.');
    } catch (err) {
      setError(err.response?.data?.error?.message || 'Không lưu được Wi-Fi.');
    } finally {
      setSavingWifi(false);
    }
  };

  const saveConfig = async () => {
    setSavingCfg(true); setError(null);
    try {
      await updateBoxConfig(boxId, cfg);
      flash('Đã lưu. Hộp áp dụng ở lần đồng bộ kế tiếp.');
      // Lấy lại config_rev mới để dòng trạng thái chuyển sang "đang chờ hộp".
      const res = await getBoxDetails(boxId);
      if (res.success) setBox(res.data);
    } catch (err) {
      setError(err.response?.data?.error?.message || 'Không lưu được cài đặt.');
    } finally {
      setSavingCfg(false);
    }
  };

  return (
    <Screen>
      <AppBar onBack={() => navigate(`/box/${boxId}/receiver`)} />
      <Body>
        <Header title="Cài đặt hộp" to={profile?.boxes_list?.[boxId]?.box_name || `Hộp ${boxId}`} />

        {error && <div className="sl-reason">{error}</div>}
        {saved && (
          <div className="sl-note" style={{ background: 'var(--success-bg)', color: 'var(--success-text)' }}>
            <Icon name="check" size={16} />
            <span>{saved}</span>
          </div>
        )}

        {loading ? (
          <span className="sl-body">Đang tải…</span>
        ) : (
          <>
            {/* --- Wi-Fi --- */}
            <label className="sl-field">
              <span className="sl-label">Tên Wi-Fi (SSID)</span>
              <input
                className="sl-input" value={ssid} maxLength={32} autoComplete="off"
                onChange={(e) => setSsid(e.target.value)} placeholder="Ví dụ: Nha_Duyen"
              />
              {/* ESP32-C3 (esp32-c3-devkitm-1) chỉ có radio 2.4 GHz — mạng 5 GHz
                  hộp không nhìn thấy. Đây là lý do hỏng hay gặp nhất khi đổi Wi-Fi. */}
              <span className="sl-caption" style={{ color: 'var(--neutral-400)' }}>
                Bắt buộc · tối đa 32 ký tự · chỉ băng tần 2.4 GHz
              </span>
            </label>

            <label className="sl-field">
              <span className="sl-label">Mật khẩu Wi-Fi</span>
              <span style={{ position: 'relative', display: 'flex', alignItems: 'center' }}>
                <input
                  className="sl-input" type={showPass ? 'text' : 'password'}
                  value={password} maxLength={63} autoComplete="new-password"
                  onChange={(e) => setPassword(e.target.value)}
                  placeholder="Để trống nếu mạng không có mật khẩu"
                  style={{ paddingRight: 44 }}
                />
                <button type="button" className="sl-iconbtn" onClick={() => setShowPass(!showPass)}
                  aria-label={showPass ? 'Ẩn mật khẩu' : 'Hiện mật khẩu'}
                  style={{ position: 'absolute', right: 4 }}>
                  <Icon name="eye" size={20} />
                </button>
              </span>
              <span className="sl-caption" style={{ color: 'var(--neutral-400)' }}>
                Tối đa 63 ký tự · để trống với mạng mở
              </span>
            </label>

            {/* Firmware hiện tại chỉ đọc a_flag + alarm_list từ DB (NetworkManager
                syncWakeup) — wifi_config và config_flag không ai đọc. Không hứa
                "hộp sẽ nhận" khi thật ra nó không nhận. */}
            <div className="sl-note sl-note--warn">
              <Icon name="wifi" size={16} />
              <span>
                Phiên bản firmware hiện tại của hộp chưa đọc Wi-Fi lưu ở đây. Muốn đổi
                ngay, dùng trang cài đặt khi hộp phát Wi-Fi riêng.
              </span>
            </div>

            <Button kind="gho" onClick={saveWifi} disabled={savingWifi}>
              {savingWifi ? 'Đang lưu…' : 'Lưu Wi-Fi'}
            </Button>

            {/* --- đèn, màn, loa --- */}
            <span className="sl-label">Đèn báo</span>
            <div className="sl-seg" style={{ flexWrap: 'wrap' }}>
              {LED_OPTIONS.map((o) => (
                <button
                  key={o.value} type="button" className="sl-seg__cell"
                  aria-pressed={cfg.led_state === o.value}
                  onClick={() => setCfg({ ...cfg, led_state: o.value })}
                  style={{ flexBasis: '40%' }}
                >
                  {o.label}{o.soon ? ' · sắp có' : ''}
                </button>
              ))}
            </div>
            {cfg.led_state === 'BREATHING' && (
              <div className="sl-note sl-note--warn">
                <Icon name="alert" size={16} />
                <span>Hiệu ứng "thở" chưa được firmware dựng xong. Chọn bây giờ thì đèn vẫn sáng đều.</span>
              </div>
            )}

            {/* Firmware kẹp độ sáng tối thiểu 5% (SETTINGS_MIN_BRIGHTNESS): kéo về 0 thì
                màn đen hẳn, người dùng tưởng hộp hỏng. Âm lượng 0 = tắt tiếng tin nhắn,
                báo thức có âm lượng riêng. */}
            <Slider label="Độ sáng màn hình" value={cfg.display_brightness} min={5}
              onChange={(v) => setCfg({ ...cfg, display_brightness: v })} />
            <Slider label="Âm lượng phát tin nhắn" value={cfg.playback_volume}
              onChange={(v) => setCfg({ ...cfg, playback_volume: v })} />

            <ApplyState config={box?.config} status={box?.status} />

            <div className="sl-note sl-note--warn">
              <Icon name="alert" size={16} />
              <span>Đèn báo chưa được firmware áp dụng. Độ sáng và âm lượng thì có.</span>
            </div>

            <Button kind="gho" onClick={saveConfig} disabled={savingCfg}>
              {savingCfg ? 'Đang lưu…' : 'Lưu đèn, màn hình và âm lượng'}
            </Button>

            {/* --- giao diện màn hình hộp: lưu lên tài khoản, xem theme/layout.js --- */}
            <button
              type="button" className="sl-listcard" style={{ cursor: 'pointer' }}
              onClick={() => navigate(`/box/${boxId}/receiver/theme`)}
            >
              <Icon name="palette" size={20} style={{ color: 'var(--chip-fg)' }} />
              <div className="sl-listcard__mid">
                <span className="sl-label-s">Giao diện màn hình hộp</span>
                <span className="sl-caption">Bố cục, phông chữ và màu trên màn 240 × 240.</span>
              </div>
              <Icon name="chevron" size={16} style={{ color: 'var(--neutral-400)' }} />
            </button>

            <SdCard status={box?.status} />

            {/* --- firmware: chỉ đọc, hộp tự cài --- */}
            <div className="sl-listcard">
              <CircleIcon size={40} bg="var(--caramel-50)" color="var(--caramel-700)" icon="gear" iconSize={20} />
              <div className="sl-listcard__mid">
                <span className="sl-label-s">Firmware {fwVersion(box?.status) || '—'}</span>
                {/* Không nói "đang là bản mới nhất / hộp tự cài": firmware không đọc
                    ota_flag, OTA giờ do người dùng kích hoạt trên hộp. */}
                <span className="sl-caption">Phiên bản hộp báo ở lần đồng bộ gần nhất.</span>
              </div>
            </div>

            {/* --- vùng nguy hiểm --- */}
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
      </Body>

      {confirmUnpair && (
        <UnpairConfirm boxId={boxId} role="receiver" onClose={() => setConfirmUnpair(false)} onError={setError} />
      )}
    </Screen>
  );
}

/**
 * Thẻ nhớ + nhật ký lỗi gần nhất (status.sd_state / sd_free_mb / log_tail do firmware gửi).
 * Thẻ lỗi thì hộp vẫn chạy: giao diện nằm trong flash, báo thức kêu tiếng bíp, chỉ không
 * tải được tin và nhạc. Cách xử lý là thay/format thẻ, phần mềm không làm gì thêm.
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
 * Hộp đã áp dụng lần lưu gần nhất chưa: backend tăng config.config_rev mỗi lần lưu,
 * hộp chép số đó vào status.config_rev sau khi ghi NVS. Hộp ngủ thì tới 5 phút mới thức.
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

/** Thanh trượt, đúng khoảng validation.middleware.ts:123-124 */
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
