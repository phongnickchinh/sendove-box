import React, { createContext, useCallback, useContext, useEffect, useState } from 'react';
import Icon from '../components/ui/Icon';
import { runSend, SendError } from '../utils/sendMessage';

/**
 * Một lượt gửi tin sống lâu hơn màn gửi. Trước đây bấm "Để sau" (hoặc rời trang) thì
 * việc tải vẫn chạy nhưng không ai báo kết quả — lỗi là mất nội dung trong im lặng.
 *
 * job: { id, boxId, boxName, input, phase: encoding|uploading|done|error, progress,
 *        summary: { fileName, duration }, errorText, detached }
 * detached = người dùng đã rời màn bước 3 → thanh trạng thái nổi đáy màn hình thay chỗ.
 *
 * Chỉ MỘT lượt mỗi lúc: gửi lượt thứ hai khi lượt đầu chưa xong bị chặn (start trả false).
 * Cho lượt sau đè lên lượt trước là quay lại đúng lỗi "hỏng mà không ai biết".
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
      // Lỗi từ backend (vd. vượt rate limit 100 tin/ngày) có message riêng.
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

  /** Rời màn bước 3: đã xong thì bỏ (giải phóng blob), còn chạy / lỗi thì chuyển ra thanh nổi. */
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

/** Thanh nổi đáy màn hình cho lượt gửi đang chạy nền. Gửi xong tự ẩn sau 5 giây. */
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
