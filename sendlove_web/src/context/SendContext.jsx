import React, { createContext, useCallback, useContext, useEffect, useState } from 'react';
import Icon from '../components/ui/Icon';
import { runSend, SendError } from '../utils/sendMessage';

/**
 * A send outlives the send screen, so leaving the page mid-upload still reports
 * the result.
 * job: { id, boxId, boxName, input, phase: encoding|uploading|done|error, progress,
 *        summary: { fileName, duration }, errorText, detached }
 * detached = the user left the step-3 screen → the floating bottom bar takes over.
 * ONE send at a time: a second start() while one is unfinished returns false.
 */
const SendCtx = createContext(null);

export const useSend = () => useContext(SendCtx);

const isRunning = (j) => j && (j.phase === 'encoding' || j.phase === 'uploading');

export function SendProvider({ children }) {
  const [job, setJob] = useState(null);

  const execute = useCallback(async (base) => {
    const id = Date.now();
    const update = (patch) => setJob((j) => (j && j.id === id ? { ...j, ...patch } : j));
    setJob({ ...base, id, phase: 'encoding', progress: 0, summary: { fileName: null, duration: 0 }, errorText: null });
    try {
      await runSend(base.boxId, base.input, {
        onPhase: (phase) => update({ phase, progress: 0 }),
        onProgress: (progress) => update({ progress }),
        onSummary: (summary) => update({ summary }),
      });
      update({ phase: 'done', progress: 100 });
    } catch (err) {
      console.error(err);
      // Backend errors (e.g. the 100 messages/day rate limit) carry their own message.
      update({
        phase: 'error',
        errorText: err instanceof SendError ? err.message : err.response?.data?.error?.message || null,
      });
    }
  }, []);

  const start = useCallback((boxId, boxName, input) => {
    if (isRunning(job)) return false;
    execute({ boxId, boxName, input, detached: false });
    return true;
  }, [job, execute]);

  const retry = useCallback(() => {
    if (!job || isRunning(job)) return;
    execute({ boxId: job.boxId, boxName: job.boxName, input: job.input, detached: job.detached });
  }, [job, execute]);

  /** Leaving step 3: drop a finished job (frees its blobs); a running / failed one moves to the floating bar. */
  const leave = useCallback(() => {
    setJob((j) => (!j ? j : j.phase === 'done' ? null : { ...j, detached: true }));
  }, []);

  const dismiss = useCallback(() => setJob(null), []);

  return (
    <SendCtx.Provider value={{ job, running: isRunning(job), start, retry, leave, dismiss }}>
      {children}
      {job?.detached && <SendStatus job={job} onRetry={retry} onDismiss={dismiss} />}
    </SendCtx.Provider>
  );
}

const TYPE_LABEL = { video: 'video', image: 'ảnh', voice: 'lời nhắn thoại', text: 'dòng chữ', static: 'tin nhắn tĩnh' };

/** Floating bottom bar for a send running in the background. Hides itself 5 seconds after success. */
function SendStatus({ job, onRetry, onDismiss }) {
  const done = job.phase === 'done';
  useEffect(() => {
    if (!done) return undefined;
    const t = setTimeout(onDismiss, 5000);
    return () => clearTimeout(t);
  }, [done, onDismiss]);

  const what = TYPE_LABEL[job.input.type] || 'tin nhắn';
  const running = isRunning(job);
  const pct = Math.round(job.progress || 0);

  return (
    <div className={`sl-sendpill sl-sendpill--${job.phase}`} role={job.phase === 'error' ? 'alert' : 'status'}>
      <div className="sl-sendpill__row">
        <Icon name={done ? 'check' : job.phase === 'error' ? 'alert' : 'up'} size={18} />
        <span className="sl-sendpill__text">
          {running && `${job.phase === 'uploading' ? 'Đang tải' : 'Đang xử lý'} ${what} cho ${job.boxName} · ${pct}%`}
          {done && `Đã gửi ${what} tới ${job.boxName}`}
          {job.phase === 'error' && `Chưa gửi được ${what} tới ${job.boxName}`}
        </span>
        {job.phase === 'error' && (
          <button type="button" className="sl-sendpill__btn" onClick={onRetry}>Thử lại</button>
        )}
        {!running && (
          <button type="button" className="sl-iconbtn" onClick={onDismiss} aria-label="Đóng">
            <Icon name="x" size={18} />
          </button>
        )}
      </div>
      {running && (
        <div className="sl-track" style={{ height: 4 }}><div className="sl-track__bar" style={{ width: `${pct}%` }} /></div>
      )}
      {job.phase === 'error' && job.errorText && <span className="sl-sendpill__err">{job.errorText}</span>}
    </div>
  );
}
