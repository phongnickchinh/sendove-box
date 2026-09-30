import React, { lazy, Suspense, useEffect, useState } from 'react';
import { useParams, useNavigate } from 'react-router-dom';
import VideoInput from '../components/sender/VideoInput';
// Kéo theo react-easy-crop — chỉ tải khi chọn thẻ Ảnh / Tin tĩnh.
const ImageInput = lazy(() => import('../components/sender/ImageInput'));
const loadingInput = <span className="sl-body">Đang tải…</span>;
import VoiceInput from '../components/sender/VoiceInput';
import TextPreview from '../components/sender/TextPreview';
import EncodingProgress from '../components/sender/EncodingProgress';
import Icon from '../components/ui/Icon';
import { Screen, AppBar, Body, Actions, Button, Header, Tips } from '../components/ui/Screen';
import { getBoxDetails } from '../api/box';
import { useAuth } from '../context/AuthContext';
import { useSend } from '../context/SendContext';
import { useToast } from '../components/ui/Toast';
import { MAX_SECONDS, maxSecondsFor } from '../utils/boxStatus';

/** Thẻ loại nội dung; hint là hàm vì trần thời lượng phụ thuộc loại hộp. */
const TYPES = [
  { key: 'video', icon: 'video', label: 'Video', hint: (max) => `Tối đa ${max} giây` },
  { key: 'image', icon: 'image', label: 'Ảnh', hint: () => 'Khung vuông' },
  { key: 'voice', icon: 'mic', label: 'Ghi âm', hint: (max) => `Thu hoặc chọn file · ${max} giây` },
  { key: 'text', icon: 'text', label: 'Văn bản', hint: () => 'Gửi được ngay' },
  { key: 'static', icon: 'layers', label: 'Tin nhắn tĩnh', hint: () => 'Ảnh, chữ, nhạc nền' },
];

const STEP2_TITLE = {
  video: 'Gửi một đoạn video',
  image: 'Gửi một bức ảnh',
  voice: 'Ghi một lời nhắn',
  text: 'Gửi một dòng chữ',
  static: 'Ảnh, chữ, nhạc nền — tuỳ bạn chọn',
};

export default function SenderUI() {
  const { boxId } = useParams();
  const navigate = useNavigate();

  const [step, setStep] = useState(1); // 1: Chọn loại, 2: Nhập nội dung, 3: Mã hoá & gửi
  const [type, setType] = useState(null); // 'video' | 'image' | 'voice' | 'text' | 'static'
  const [text, setText] = useState('');

  // Card "Tin nhắn tĩnh": giữ tạm ảnh/nhạc nền đã chọn cho tới khi bấm Gửi
  // chung — khác với các card cũ (ảnh/voice riêng lẻ) tự upload ngay khi xong.
  const [staticImageBlob, setStaticImageBlob] = useState(null);
  const [staticAudioData, setStaticAudioData] = useState(null); // { wavBlob, duration }

  // Lượt gửi sống trong SendContext (sống lâu hơn trang này): "Để sau" hay rời trang thì
  // thanh nổi đáy màn hình báo tiến độ và kết quả, lỗi thì có nút Thử lại.
  const { job, running, start, retry, leave } = useSend();
  const [toast, showToast] = useToast();

  const { profile } = useAuth();
  const boxName = profile?.boxes_list?.[boxId]?.box_name || `Hộp ${boxId}`;

  // Trần thời lượng theo loại bộ nhớ của hộp. Chưa đọc được hộp thì tạm dùng
  // mức NAND (thấp hơn) — an toàn cho mọi hộp; đọc xong mới nới lên.
  const [maxSeconds, setMaxSeconds] = useState(MAX_SECONDS.nand);
  useEffect(() => {
    let alive = true;
    getBoxDetails(boxId)
      .then((res) => {
        if (!alive || !res.success) return;
        setMaxSeconds(maxSecondsFor(res.data));
      })
      .catch(() => {});
    return () => { alive = false; };
  }, [boxId]);

  // Ba bước cùng một route: đổi bước thì tự cuộn lên đầu, không thì bước 2 mở ra ở vị
  // trí cuộn của lưới thẻ bước 1 và che mất tiêu đề + ô lời nhắn.
  useEffect(() => { window.scrollTo(0, 0); }, [step]);

  const handleTypeSelect = (selectedType) => {
    setType(selectedType);
    setStep(2);
  };

  const handleCancel = () => {
    setStep(1);
    setType(null);
    setText('');
    setStaticImageBlob(null);
    setStaticAudioData(null);
  };

  const processAndUpload = (mediaData, range) => {
    if (running) {
      showToast('Tin trước vẫn đang gửi. Đợi xong rồi gửi tiếp nhé.', 'err');
      return;
    }
    start(boxId, boxName, { type, text, mediaData, range });
    setStep(3);
  };

  // Rời trang khi bước 3 còn mở (nút back trình duyệt, bấm link khác): chuyển lượt gửi ra
  // thanh nổi thay vì để nó chạy mà không ai thấy.
  useEffect(() => () => leave(), [leave]);

  // ---------- Bước 3: popup mã hoá / màn kết quả ----------
  const mine = job && !job.detached && job.boxId === boxId;
  if (step === 3 && mine) {
    return (
      <EncodingProgress
        phase={job.phase}
        progress={Math.round(job.progress)}
        type={job.input.type}
        duration={job.summary.duration}
        fileName={job.summary.fileName}
        boxName={boxName}
        onHome={() => { leave(); navigate('/dashboard'); }}
        onRetry={retry}
        onSendAnother={() => { leave(); handleCancel(); }}
        onLeave={() => { leave(); handleCancel(); }}
        errorText={job.errorText}
      />
    );
  }

  // ---------- Bước 1: chọn loại nội dung ----------
  // step 3 mà lượt gửi đã được bỏ/chuyển ra thanh nổi → về lưới chọn loại.
  if (step === 1 || step === 3) {
    return (
      <Screen>
        <AppBar step="Bước 1/3" onBack={() => navigate('/dashboard')} />
        <Body>
          <Header title="Gửi yêu thương" to={boxName} />

          <div className="sl-grid2">
            {TYPES.map((t) => (
              <button
                key={t.key}
                type="button"
                className="sl-typecard"
                onClick={() => handleTypeSelect(t.key)}
              >
                <span className="sl-chip">
                  <Icon name={t.icon} size={24} />
                </span>
                <span className="sl-label-s">{t.label}</span>
                <span className="sl-caption">{t.hint(maxSeconds)}</span>
              </button>
            ))}
          </div>

          <Actions>
            <Button kind="sec" onClick={() => navigate(`/box/${boxId}/sender/dashboard`)}>
              Lịch sử tin nhắn
            </Button>
          </Actions>
        </Body>
      </Screen>
    );
  }

  // ---------- Bước 2: nhập nội dung ----------
  return (
    <Screen>
      <AppBar step="Bước 2/3" onBack={handleCancel} />
      <Body>
        <Header title={STEP2_TITLE[type]} to={boxName} />

        {/* Màn ghi âm KHÔNG có ô nhập lời nhắn — loại voice không mang text. */}
        {type !== 'voice' && type !== 'text' && (
          <div className="sl-field">
            <label className="sl-label" htmlFor="sl-note">Lời nhắn (tuỳ chọn)</label>
            <textarea
              id="sl-note"
              className="sl-input"
              rows={3}
              placeholder="Vài chữ gửi kèm..."
              value={text}
              onChange={(e) => setText(e.target.value)}
            />
          </div>
        )}

        {type === 'video' && <VideoInput onVideoSelect={processAndUpload} onCancel={handleCancel} maxSeconds={maxSeconds} />}
        {type === 'image' && (
          <Suspense fallback={loadingInput}>
            <ImageInput onImageSelect={processAndUpload} onCancel={handleCancel} tip={null} />
          </Suspense>
        )}
        {type === 'voice' && <VoiceInput onRecordComplete={processAndUpload} onCancel={handleCancel} maxSeconds={maxSeconds} />}

        {type === 'text' && (
          <>
            <div className="sl-field">
              <label className="sl-label" htmlFor="sl-text">Lời nhắn của bạn</label>
              <textarea
                id="sl-text"
                className="sl-input"
                rows={6}
                placeholder="Viết điều bạn muốn hiện lên màn hình hộp"
                value={text}
                onChange={(e) => setText(e.target.value)}
              />
            </div>

            <TextPreview text={text} />

            <Actions>
              <Button kind="pri" disabled={!text.trim()} onClick={() => processAndUpload(null)}>
                Gửi
              </Button>
              <Button kind="gho" onClick={handleCancel}>Huỷ</Button>
            </Actions>
          </>
        )}

        {type === 'static' && (
          <>
            {/* Ảnh (tuỳ chọn) */}
            {!staticImageBlob ? (
              <Suspense fallback={loadingInput}>
                <ImageInput onImageSelect={setStaticImageBlob} onCancel={handleCancel} />
              </Suspense>
            ) : (
              <div className="sl-card sl-card--center">
                <span className="sl-chip"><Icon name="image" size={24} /></span>
                <span className="sl-caption">Đã chọn ảnh</span>
                <Actions>
                  <Button kind="gho" onClick={() => setStaticImageBlob(null)}>Đổi ảnh</Button>
                </Actions>
              </div>
            )}

            {/* Nhạc nền (tuỳ chọn) */}
            {!staticAudioData ? (
              <VoiceInput onRecordComplete={setStaticAudioData} onCancel={handleCancel} maxSeconds={maxSeconds} purpose="music" />
            ) : (
              <div className="sl-card sl-card--center">
                <span className="sl-chip"><Icon name="mic" size={24} /></span>
                <span className="sl-caption">Đã chọn nhạc nền {staticAudioData.duration}s</span>
                <Actions>
                  <Button kind="gho" onClick={() => setStaticAudioData(null)}>Đổi nhạc nền</Button>
                </Actions>
              </div>
            )}

            <Tips>Chọn ít nhất 1 trong 3: ảnh, chữ, hoặc nhạc nền — không bắt buộc đủ cả 3.</Tips>

            <Actions>
              <Button
                kind="pri"
                disabled={!staticImageBlob && !staticAudioData && !text.trim()}
                onClick={() => processAndUpload({ imageBlob: staticImageBlob, audioData: staticAudioData })}
              >
                Gửi
              </Button>
              <Button kind="gho" onClick={handleCancel}>Huỷ</Button>
            </Actions>
          </>
        )}
      </Body>
      {toast}
    </Screen>
  );
}
