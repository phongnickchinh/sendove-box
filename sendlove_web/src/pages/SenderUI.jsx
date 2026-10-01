import React, { lazy, Suspense, useEffect, useState } from 'react';
import { useParams, useNavigate } from 'react-router-dom';
import VideoInput from '../components/sender/VideoInput';
// Pulls in react-easy-crop — loaded only when the Image / Still card is picked.
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

/** Content-type cards; hint is a function because the duration cap depends on the box type. */
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

  const [step, setStep] = useState(1); // 1: pick type, 2: enter content, 3: encode & send
  const [type, setType] = useState(null); // 'video' | 'image' | 'voice' | 'text' | 'static'
  const [text, setText] = useState('');

  // "Still message" card: holds the chosen image / background music until the
  // shared Send button is pressed — unlike the single-media cards, which send
  // as soon as their input is done.
  const [staticImageBlob, setStaticImageBlob] = useState(null);
  const [staticAudioData, setStaticAudioData] = useState(null); // { wavBlob, duration }

  // The send lives in SendContext (it outlives this page): after "Later" or
  // leaving the page, the floating bottom bar reports progress and the result,
  // with a Retry button on failure.
  const { job, running, start, retry, leave } = useSend();
  const [toast, showToast] = useToast();

  const { profile } = useAuth();
  const boxName = profile?.boxes_list?.[boxId]?.box_name || `Hộp ${boxId}`;

  // Duration cap by the box's storage type. Until the box is loaded, use the
  // (lower) NAND cap — safe for every box — and raise it afterwards.
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

  // Three steps share one route: scroll to the top on a step change, or step 2
  // opens at step 1's scroll position and hides the title + caption field.
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

  // Leaving the page while step 3 is open (browser back, another link): hand
  // the send to the floating bar instead of letting it run unseen.
  useEffect(() => () => leave(), [leave]);

  // ---------- Step 3: encoding popup / result screen ----------
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

  // ---------- Step 1: pick the content type ----------
  // On step 3 with the send already dropped / moved to the floating bar → back to the type grid.
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

  // ---------- Step 2: enter content ----------
  return (
    <Screen>
      <AppBar step="Bước 2/3" onBack={handleCancel} />
      <Body>
        <Header title={STEP2_TITLE[type]} to={boxName} />

        {/* The recording screen has NO caption field — voice messages carry no text. */}
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
            {/* Image (optional) */}
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

            {/* Background music (optional) */}
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
