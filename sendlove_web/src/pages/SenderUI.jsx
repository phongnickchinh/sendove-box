import React, { useEffect, useRef, useState } from 'react';
import { useParams, useNavigate } from 'react-router-dom';
import VideoInput from '../components/sender/VideoInput';
import ImageInput from '../components/sender/ImageInput';
import VoiceInput from '../components/sender/VoiceInput';
import EncodingProgress from '../components/sender/EncodingProgress';
import Icon from '../components/ui/Icon';
import { Screen, AppBar, Body, Actions, Button, Header, Tips } from '../components/ui/Screen';
import { getBoxDetails } from '../api/box';
import { useAuth } from '../context/AuthContext';
import { encodeVideoToBin, encodeImageToBin, extractAudioFromVideo } from '../utils/mediaEncoder';
import { uploadMessage } from '../utils/mediaUploader';
import { FW_MAX_MEDIA_BYTES, MAX_SECONDS, maxAudioSecondsFor, maxBinBytesFor, maxSecondsFor } from '../utils/boxStatus';

/** Thẻ loại nội dung; hint là hàm vì trần thời lượng phụ thuộc loại hộp. */
const TYPES = [
  { key: 'video', icon: 'video', label: 'Video', hint: (max) => `Tối đa ${max} giây` },
  { key: 'image', icon: 'image', label: 'Ảnh', hint: () => 'Khung vuông' },
  { key: 'voice', icon: 'mic', label: 'Ghi âm', hint: (max) => `Thu hoặc chọn file · ${max} giây` },
  { key: 'text', icon: 'text', label: 'Văn bản', hint: () => 'Gửi được ngay' },
  { key: 'static', icon: 'layers', label: 'Tin nhắn tĩnh', hint: () => 'Ảnh, chữ, nhạc nền' },
];

/** Lỗi đọc được cho người dùng; lỗi lạ thì rơi về câu mặc định của EncodingProgress. */
class SendError extends Error {}

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

  // Encoding & Uploading states
  const [phase, setPhase] = useState('encoding'); // 'encoding' | 'uploading' | 'done' | 'error'
  const [progress, setProgress] = useState(0);
  // Thông tin hiển thị trên thẻ tóm tắt của bước 3
  const [summary, setSummary] = useState({ fileName: null, duration: 0 });
  // Giữ lại media của lần gửi gần nhất để nút "Thử lại" gửi lại đúng file đó,
  // đúng như dòng "còn giữ trên máy này" ở màn báo lỗi.
  const [lastMedia, setLastMedia] = useState(null);
  // Mỗi lần gửi có một số thứ tự; promise của lần cũ không được ghi đè
  // trạng thái của lần mới (xảy ra khi người dùng bấm "Để sau" rồi gửi tiếp).
  const sendIdRef = useRef(0);
  const [errorText, setErrorText] = useState(null);

  const { profile } = useAuth();
  const boxName = profile?.boxes_list?.[boxId]?.box_name || `Hộp ${boxId}`;

  // Trần thời lượng theo loại bộ nhớ của hộp. Chưa đọc được hộp thì tạm dùng
  // mức NAND (thấp hơn) — an toàn cho mọi hộp; đọc xong mới nới lên.
  const [maxSeconds, setMaxSeconds] = useState(MAX_SECONDS.nand);
  const [maxBinBytes, setMaxBinBytes] = useState(FW_MAX_MEDIA_BYTES);
  // Tin thoại / nhạc nền: âm thanh là toàn bộ nội dung, vượt trần PCM hộp phát
  // câm — nên trần của chúng là min(trần loại hộp, trần âm thanh firmware).
  const [maxAudioSeconds, setMaxAudioSeconds] = useState(maxAudioSecondsFor(null));
  const voiceMax = Math.min(maxSeconds, maxAudioSeconds);
  useEffect(() => {
    let alive = true;
    getBoxDetails(boxId)
      .then((res) => {
        if (!alive || !res.success) return;
        setMaxSeconds(maxSecondsFor(res.data));
        setMaxBinBytes(maxBinBytesFor(res.data));
        setMaxAudioSeconds(maxAudioSecondsFor(res.data));
      })
      .catch(() => {});
    return () => { alive = false; };
  }, [boxId]);

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

  const processAndUpload = async (mediaData, range) => {
    const sendId = ++sendIdRef.current;
    const alive = () => sendIdRef.current === sendId;
    const setProgressIfAlive = (v) => { if (alive()) setProgress(v); };

    setLastMedia({ mediaData, range });
    setStep(3);
    setPhase('encoding');
    setProgress(0);
    setErrorText(null);

    try {
      let payload = { type, text };

      if (type === 'video') {
        const file = mediaData;
        setSummary({ fileName: file.name, duration: 0 });
        const encodeRes = await encodeVideoToBin(file, setProgressIfAlive, range);
        // Chặn TRƯỚC khi tải lên: tin quá trần của hộp sẽ kẹt ở hộp và chặn
        // mọi tin sau nó (xem FW_MAX_MEDIA_BYTES).
        if (encodeRes.binBlob.size > maxBinBytes) {
          const mb = (b) => (b / 1e6).toFixed(1).replace('.', ',');
          const fitSecs = Math.max(1, Math.floor(encodeRes.duration * (maxBinBytes / encodeRes.binBlob.size)));
          throw new SendError(
            `Đoạn video sau khi nén nặng ${mb(encodeRes.binBlob.size)} MB, hộp hiện chỉ nhận tối đa `
            + `${mb(maxBinBytes)} MB mỗi tin. Chọn đoạn ngắn hơn — khoảng ${fitSecs} giây là vừa.`,
          );
        }
        setSummary({ fileName: file.name, duration: encodeRes.duration });
        setProgress(0); // Reset progress cho bước trích xuất âm thanh
        const voiceBlob = await extractAudioFromVideo(file, setProgressIfAlive, range);

        payload = {
          ...payload,
          binBlob: encodeRes.binBlob,
          thumbBlob: encodeRes.thumbBlob,
          voiceBlob,
          originalBlob: file,
          metadata: {
            duration: encodeRes.duration,
            frameCount: encodeRes.frameCount,
            width: 240,
            height: 240
          }
        };
      } else if (type === 'image') {
        const file = mediaData;
        const encodeRes = await encodeImageToBin(file);

        payload = {
          ...payload,
          binBlob: encodeRes.binBlob,
          thumbBlob: encodeRes.thumbBlob,
          originalBlob: file,
          metadata: {
            frameCount: 1,
            width: 240,
            height: 240
          }
        };
      } else if (type === 'voice') {
        const { wavBlob, duration } = mediaData; // from VoiceRecorder
        setSummary({ fileName: null, duration });

        payload = {
          ...payload,
          voiceBlob: wavBlob,
          metadata: { duration }
        };
      } else if (type === 'static') {
        // Tin nhắn tĩnh: tuỳ tổ hợp ảnh / text / nhạc nền — tái dùng type "image"
        // có sẵn ở backend (không thêm enum mới). mediaData = { imageBlob, audioData }.
        const { imageBlob, audioData } = mediaData || {};
        let extra = {};

        if (imageBlob) {
          const encodeRes = await encodeImageToBin(imageBlob);
          extra = {
            ...extra,
            binBlob: encodeRes.binBlob,
            thumbBlob: encodeRes.thumbBlob,
            metadata: { frameCount: 1, width: 240, height: 240 }
          };
        }
        if (audioData?.wavBlob) {
          extra = { ...extra, bgMusicBlob: audioData.wavBlob };
          setSummary({ fileName: null, duration: audioData.duration });
        }

        payload = {
          ...payload,
          // Chỉ có chữ thì gửi đúng là tin chữ — type 'image' không kèm ảnh nào
          // làm lịch sử và hộp tưởng có ảnh.
          type: imageBlob || audioData?.wavBlob ? 'image' : 'text',
          ...extra,
        };
      }

      setPhase('uploading');
      setProgress(0);

      await uploadMessage(boxId, payload, setProgressIfAlive);

      if (alive()) setPhase('done');
    } catch (err) {
      console.error(err);
      if (!alive()) return;
      // Lỗi từ backend (vd. vượt rate limit 100 tin/ngày) có message riêng.
      setErrorText(err instanceof SendError ? err.message : err.response?.data?.error?.message || null);
      setPhase('error');
    }
  };

  // ---------- Bước 3: popup mã hoá / màn kết quả ----------
  if (step === 3) {
    return (
      <EncodingProgress
        phase={phase}
        progress={progress}
        type={type}
        duration={summary.duration}
        fileName={summary.fileName}
        boxName={boxName}
        onHome={() => navigate('/dashboard')}
        onRetry={() => processAndUpload(lastMedia?.mediaData, lastMedia?.range)}
        onSendAnother={handleCancel}
        onLeave={handleCancel}
        errorText={errorText}
      />
    );
  }

  // ---------- Bước 1: chọn loại nội dung ----------
  if (step === 1) {
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
                <span className="sl-caption">{t.hint(t.key === 'voice' ? voiceMax : maxSeconds)}</span>
              </button>
            ))}
          </div>

          <Tips>Nội dung chỉ hiện trên hộp của bạn, không đăng ở đâu khác.</Tips>

          <Actions>
            <Button kind="gho" onClick={() => navigate(`/box/${boxId}/sender/dashboard`)}>
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
        {type === 'image' && <ImageInput onImageSelect={processAndUpload} onCancel={handleCancel} />}
        {type === 'voice' && <VoiceInput onRecordComplete={processAndUpload} onCancel={handleCancel} maxSeconds={voiceMax} />}

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

            <div className="sl-card sl-card--center">
              <span className="sl-chip"><Icon name="text" size={24} /></span>
              <span className="sl-caption">
                Chỉ có chữ — không cần mã hoá nên gửi được ngay.
              </span>
            </div>

            <Tips>Hiện trên màn hình 240x240 của hộp.</Tips>

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
              <ImageInput onImageSelect={setStaticImageBlob} onCancel={handleCancel} />
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
              <VoiceInput onRecordComplete={setStaticAudioData} onCancel={handleCancel} maxSeconds={voiceMax} purpose="music" />
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
    </Screen>
  );
}
