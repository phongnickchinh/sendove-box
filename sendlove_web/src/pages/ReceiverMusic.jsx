import React, { useCallback, useEffect, useRef, useState } from 'react';
import { useParams, useNavigate } from 'react-router-dom';
import { listMusic, uploadMusic, renameMusic, deleteMusic, getMusicPreviewUrl } from '../api/music';
import { decodeAudioBlob, encodeAlarmMusic, audFileToWavBlob, ALARM_MUSIC } from '../utils/mediaEncoder';
import { useAuth } from '../context/AuthContext';
import Icon from '../components/ui/Icon';
import { Screen, AppBar, Body, Header, Button, Modal } from '../components/ui/Screen';
import Illustration from '../components/ui/Illustration';

/**
 * The box's alarm music library (SD-card design, firmware MEMORY.md §28).
 *
 * Box rules the UI must state plainly:
 *   10 tracks per box, 5–60 seconds each (the box rings for at most a minute,
 *   looping shorter tracks). Music downloads to the box only when an alarm
 *   uses it; once downloaded it stays on the card and isn't fetched again.
 *   Decoding + cutting + conversion to 16 kHz mono happen IN THE BROWSER (the
 *   "decode on the client" rule); the backend only validates the file.
 */

const MAX_TRACKS = 10;
const NAME_MAX = 40;

const fmt = (sec) => `${Math.floor(sec / 60)}:${String(Math.round(sec % 60)).padStart(2, '0')}`;

export default function ReceiverMusic() {
  const { boxId } = useParams();
  const navigate = useNavigate();
  const { profile } = useAuth();

  const [tracks, setTracks] = useState([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState(null);
  const [notice, setNotice] = useState(null);
  const [adding, setAdding] = useState(null);    // null | { musicId?, name? } — add new or replace a file
  const [renaming, setRenaming] = useState(null); // null | { id, name }
  const [deleting, setDeleting] = useState(null); // null | track
  const [busy, setBusy] = useState(false);
  const [playing, setPlaying] = useState(null); // the track open in the player popup

  const load = useCallback(async () => {
    try {
      const res = await listMusic(boxId);
      if (res.success) setTracks(res.data || []);
      setError(null);
    } catch {
      setError('Không đọc được thư viện nhạc.');
    } finally {
      setLoading(false);
    }
  }, [boxId]);

  useEffect(() => { load(); }, [load]);

  const full = tracks.length >= MAX_TRACKS;

  const doRename = async () => {
    setBusy(true);
    try {
      await renameMusic(boxId, renaming.id, renaming.name.trim());
      setRenaming(null);
      await load();
    } catch (err) {
      setError(err.response?.data?.error?.message || 'Không đổi được tên.');
    } finally {
      setBusy(false);
    }
  };

  const doDelete = async () => {
    setBusy(true);
    try {
      const res = await deleteMusic(boxId, deleting.music_id);
      const n = res.data?.detached_alarms || 0;
      setNotice(n > 0 ? `Đã xoá. ${n} báo thức đang dùng bài này chuyển về tiếng bíp.` : 'Đã xoá bài nhạc.');
      setDeleting(null);
      await load();
    } catch {
      setError('Không xoá được bài nhạc.');
    } finally {
      setBusy(false);
    }
  };

  return (
    <Screen>
      <AppBar onBack={() => navigate(`/box/${boxId}/receiver/alarm`)} />
      <Body>
        <Header title="Nhạc báo thức" to={profile?.boxes_list?.[boxId]?.box_name || `Hộp ${boxId}`} />

        <span className="sl-caption" style={{ fontWeight: 500, color: full ? 'var(--warning-text)' : 'var(--neutral-500)' }}>
          Đã dùng {tracks.length}/{MAX_TRACKS} bài
        </span>

        {error && <div className="sl-reason">{error}</div>}
        {notice && (
          <div className="sl-note" style={{ background: 'var(--success-bg)', color: 'var(--success-text)' }}>
            <Icon name="check" size={16} />
            <span>{notice}</span>
          </div>
        )}

        {loading ? (
          <span className="sl-body">Đang tải…</span>
        ) : tracks.length === 0 ? (
          <div className="sl-card sl-card--center">
            <Illustration name="music" />
            <span className="sl-heading">Chưa có bài nhạc nào</span>
            <span className="sl-body">Thêm một đoạn nhạc từ máy của bạn để hộp kêu bằng bài đó thay cho tiếng bíp.</span>
          </div>
        ) : (
          <div style={{ display: 'flex', flexDirection: 'column', gap: 10 }}>
            {tracks.map((t) => (
              <div className="sl-listcard" key={t.music_id} style={{ gap: 'var(--sp-2)', padding: '10px var(--sp-2) 10px var(--sp-4)' }}>
                <Icon name="bell" size={20} style={{ color: 'var(--chip-fg)', flex: '0 0 auto' }} />
                <div className="sl-listcard__mid">
                  <span className="sl-label-s" style={{ overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>{t.name}</span>
                  <span className="sl-caption">{fmt(t.duration_ms / 1000)}</span>
                </div>
                <button type="button" className="sl-iconbtn sl-iconbtn--44" aria-label={`Nghe ${t.name}`} title="Nghe"
                  onClick={() => setPlaying(t)}>
                  <Icon name="play" size={20} />
                </button>
                <button type="button" className="sl-iconbtn sl-iconbtn--44" aria-label={`Đổi tên ${t.name}`} title="Đổi tên"
                  onClick={() => setRenaming({ id: t.music_id, name: t.name })}>
                  <Icon name="pencil" size={20} />
                </button>
                <button type="button" className="sl-iconbtn sl-iconbtn--44" aria-label={`Thay file ${t.name}`} title="Thay file"
                  onClick={() => setAdding({ musicId: t.music_id, name: t.name })}>
                  <Icon name="replace" size={20} />
                </button>
                <button type="button" className="sl-iconbtn sl-iconbtn--44" aria-label={`Xoá ${t.name}`} title="Xoá"
                  onClick={() => setDeleting(t)} style={{ color: 'var(--error-text)' }}>
                  <Icon name="trash" size={20} />
                </button>
              </div>
            ))}
          </div>
        )}

        <button type="button" className="sl-addrow" disabled={full} onClick={() => setAdding({})}>
          <Icon name="plus" size={20} />
          {full ? 'Thư viện nhạc đã đầy' : 'Thêm bài nhạc'}
        </button>

      </Body>

      {adding && (
        <AddMusicModal
          boxId={boxId}
          initial={adding}
          onClose={() => setAdding(null)}
          onSaved={async (m) => {
            setAdding(null);
            setNotice(`Đã lưu "${m.name}". Hộp tải về ở lần đồng bộ kế tiếp nếu có báo thức dùng bài này.`);
            await load();
          }}
        />
      )}

      {playing && <MusicPlayer boxId={boxId} track={playing} onClose={() => setPlaying(null)} />}

      {renaming && (
        <Modal onClose={busy ? undefined : () => setRenaming(null)}>
          <span className="sl-heading">Đổi tên bài nhạc</span>
          <label className="sl-field">
            <span className="sl-label">Tên</span>
            <input className="sl-input" value={renaming.name} maxLength={NAME_MAX}
              onChange={(e) => setRenaming({ ...renaming, name: e.target.value })} />
          </label>
          <Button kind="pri" onClick={doRename} disabled={busy || !renaming.name.trim()}>
            {busy ? 'Đang lưu…' : 'Lưu tên'}
          </Button>
          <Button kind="gho" onClick={() => setRenaming(null)} disabled={busy}>Huỷ</Button>
        </Modal>
      )}

      {deleting && (
        <Modal onClose={busy ? undefined : () => setDeleting(null)}>
          <span className="sl-heading">Xoá "{deleting.name}"?</span>
          <span className="sl-body">Báo thức đang dùng bài này sẽ chuyển về tiếng bíp.</span>
          <Button kind="pri" onClick={doDelete} disabled={busy} style={{ background: 'var(--error-fill)' }}>
            {busy ? 'Đang xoá…' : 'Xoá bài nhạc'}
          </Button>
          <Button kind="gho" onClick={() => setDeleting(null)} disabled={busy}>Huỷ</Button>
        </Modal>
      )}
    </Screen>
  );
}

/**
 * Player popup for a library track: name, progress bar, play/pause, stop.
 * Closing it (button, Esc, backdrop tap) stops playback and revokes the object URL.
 */
function MusicPlayer({ boxId, track, onClose }) {
  const [state, setState] = useState('loading'); // loading | ready | error
  const [paused, setPaused] = useState(true);
  const [pos, setPos] = useState(0);
  const [dur, setDur] = useState(track.duration_ms / 1000);
  const audioRef = useRef(null);

  useEffect(() => {
    let alive = true;
    let url = null;
    (async () => {
      try {
        const res = await getMusicPreviewUrl(boxId, track.music_id);
        const buf = await (await fetch(res.data.url)).arrayBuffer();
        if (!alive) return;
        url = URL.createObjectURL(await audFileToWavBlob(buf));
        const a = new Audio(url);
        a.ontimeupdate = () => setPos(a.currentTime);
        a.onloadedmetadata = () => Number.isFinite(a.duration) && setDur(a.duration);
        a.onplay = () => setPaused(false);
        a.onpause = () => setPaused(true);
        a.onended = () => { setPaused(true); setPos(0); };
        audioRef.current = a;
        setState('ready');
        a.play().catch(() => {}); // if the browser blocks autoplay, the user taps play
      } catch {
        if (alive) setState('error');
      }
    })();
    return () => {
      alive = false;
      audioRef.current?.pause();
      audioRef.current = null;
      if (url) URL.revokeObjectURL(url);
    };
  }, [boxId, track.music_id]);

  const toggle = () => {
    const a = audioRef.current;
    if (!a) return;
    if (a.paused) a.play().catch(() => {});
    else a.pause();
  };

  const stop = () => {
    const a = audioRef.current;
    if (!a) return;
    a.pause();
    a.currentTime = 0;
    setPos(0);
  };

  const pct = dur > 0 ? Math.min(100, (pos / dur) * 100) : 0;

  return (
    <Modal onClose={onClose}>
      <div style={{ display: 'flex', alignItems: 'center', gap: 'var(--sp-2)' }}>
        <Icon name="bell" size={20} style={{ color: 'var(--chip-fg)', flex: '0 0 auto' }} />
        <span className="sl-heading" style={{ flex: 1, minWidth: 0, overflowWrap: 'anywhere' }}>{track.name}</span>
        <button type="button" className="sl-iconbtn" onClick={onClose} aria-label="Đóng">
          <Icon name="x" size={20} />
        </button>
      </div>

      {state === 'error' ? (
        <div className="sl-reason">Không phát được bài này. Kiểm tra kết nối rồi thử lại.</div>
      ) : (
        <>
          <div className="sl-progress">
            <div className="sl-track"><div className="sl-track__bar" style={{ width: `${pct}%`, transition: 'none' }} /></div>
            <span className="sl-player__times">
              <span>{fmt(pos)}</span><span>{fmt(dur)}</span>
            </span>
          </div>
          <div className="sl-player__ctrls">
            <button type="button" className="sl-iconbtn sl-iconbtn--44" onClick={stop}
              disabled={state !== 'ready'} aria-label="Dừng">
              <Icon name="stop" size={20} />
            </button>
            <button type="button" className="sl-player__main" onClick={toggle}
              disabled={state !== 'ready'} aria-label={paused ? 'Phát' : 'Tạm dừng'}>
              {state === 'loading'
                ? <span className="sl-caption-s">…</span>
                : <Icon name={paused ? 'play' : 'pause'} size={24} sw={2} />}
            </button>
            <span style={{ width: 44 }} aria-hidden="true" />
          </div>
        </>
      )}
    </Modal>
  );
}

/**
 * Pick a file → decode → choose a segment (start + 5–60s length) → preview →
 * encode to 16 kHz + AUDC → upload. With initial.musicId it replaces that track.
 */
function AddMusicModal({ boxId, initial, onClose, onSaved }) {
  const [decoded, setDecoded] = useState(null);
  const [fileName, setFileName] = useState('');
  const [name, setName] = useState(initial.name || '');
  const [start, setStart] = useState(0);
  const [length, setLength] = useState(30);
  const [stage, setStage] = useState('pick'); // pick | decoding | ready | saving
  const [progress, setProgress] = useState(0);
  const [error, setError] = useState(null);
  const playRef = useRef(null);

  useEffect(() => () => playRef.current?.stop?.(), []);

  const pick = async (e) => {
    const file = e.target.files?.[0];
    if (!file) return;
    setError(null);
    // Reject BEFORE decoding: decodeAudioData expands the whole track into RAM
    // (mandatory case, firmware MEMORY.md §28).
    if (file.size > ALARM_MUSIC.MAX_FILE_BYTES) {
      setError('File lớn hơn 15 MB. Hãy chọn file ngắn hơn hoặc nén lại.');
      return;
    }
    setStage('decoding');
    try {
      const buf = await decodeAudioBlob(file);
      if (buf.duration < ALARM_MUSIC.MIN_S) {
        setError('Bài nhạc phải dài ít nhất 5 giây.');
        setStage('pick');
        return;
      }
      setDecoded(buf);
      setFileName(file.name);
      if (!name) setName(file.name.replace(/\.[^.]+$/, '').slice(0, NAME_MAX));
      setStart(0);
      setLength(Math.min(30, Math.floor(buf.duration)));
      setStage('ready');
    } catch {
      setError('Trình duyệt không đọc được file này. Hãy thử file mp3 hoặc m4a.');
      setStage('pick');
    }
  };

  const maxLen = decoded ? Math.min(ALARM_MUSIC.MAX_S, Math.floor(decoded.duration - start)) : ALARM_MUSIC.MAX_S;
  const len = Math.max(ALARM_MUSIC.MIN_S, Math.min(length, maxLen));
  const canSave = stage === 'ready' && name.trim() && maxLen >= ALARM_MUSIC.MIN_S;

  const listen = () => {
    playRef.current?.stop?.();
    const ctx = new (window.AudioContext || window.webkitAudioContext)();
    const src = ctx.createBufferSource();
    src.buffer = decoded;
    src.connect(ctx.destination);
    src.onended = () => ctx.close();
    src.start(0, start, len);
    playRef.current = src;
  };

  const save = async () => {
    playRef.current?.stop?.();
    setStage('saving');
    setError(null);
    try {
      const { blob, durationMs } = await encodeAlarmMusic(decoded, { start, end: start + len });
      const m = await uploadMusic(boxId, blob, { name: name.trim(), durationMs, musicId: initial.musicId }, setProgress);
      onSaved(m);
    } catch (err) {
      setError(err.response?.data?.error?.message || 'Không lưu được bài nhạc.');
      setStage('ready');
    }
  };

  return (
    <Modal onClose={stage === 'saving' ? undefined : onClose}>
      <span className="sl-heading">{initial.musicId ? 'Thay file nhạc' : 'Thêm bài nhạc'}</span>
      {error && <div className="sl-reason">{error}</div>}

      <label className="sl-field">
        <span className="sl-label">File nhạc trên máy</span>
        <input type="file" accept="audio/*" onChange={pick} disabled={stage === 'decoding' || stage === 'saving'} />
        <span className="sl-caption" style={{ color: 'var(--neutral-400)' }}>
          {fileName || 'mp3, m4a, wav… tối đa 15 MB'}
        </span>
      </label>

      {stage === 'decoding' && <span className="sl-body">Đang đọc file…</span>}

      {decoded && stage !== 'decoding' && (
        <>
          <label className="sl-field">
            <span className="sl-label">Tên bài</span>
            <input className="sl-input" value={name} maxLength={NAME_MAX} onChange={(e) => setName(e.target.value)} />
          </label>

          <label className="sl-field">
            <span style={{ display: 'flex', justifyContent: 'space-between' }}>
              <span className="sl-label">Bắt đầu từ</span>
              <span className="sl-label-s">{fmt(start)} / {fmt(decoded.duration)}</span>
            </span>
            <input type="range" className="sl-range" min={0}
              max={Math.max(0, Math.floor(decoded.duration - ALARM_MUSIC.MIN_S))} step={1} value={start}
              onChange={(e) => setStart(Number(e.target.value))} />
          </label>

          <label className="sl-field">
            <span style={{ display: 'flex', justifyContent: 'space-between' }}>
              <span className="sl-label">Độ dài đoạn</span>
              <span className="sl-label-s">{len} giây</span>
            </span>
            <input type="range" className="sl-range" min={ALARM_MUSIC.MIN_S} max={Math.max(ALARM_MUSIC.MIN_S, maxLen)}
              step={1} value={len} onChange={(e) => setLength(Number(e.target.value))} />
          </label>

          <span className="sl-caption">
            Hộp kêu tối đa 1 phút; đoạn ngắn hơn sẽ phát lặp lại. Âm thanh được đổi sang 16 kHz mono cho loa của hộp.
          </span>

          <Button kind="sec" onClick={listen} disabled={stage === 'saving'}>Nghe thử đoạn này</Button>
        </>
      )}

      <Button kind="pri" onClick={save} disabled={!canSave}>
        {stage === 'saving' ? `Đang tải lên… ${Math.round(progress)}%` : 'Lưu bài nhạc'}
      </Button>
      <Button kind="gho" onClick={onClose} disabled={stage === 'saving'}>Huỷ</Button>
    </Modal>
  );
}
