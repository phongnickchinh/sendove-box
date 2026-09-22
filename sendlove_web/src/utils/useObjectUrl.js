import { useEffect, useState } from 'react';

/**
 * blob → object URL, tự revoke khi blob đổi hoặc component rời màn.
 *
 * Tạo URL TRONG effect, không trong useMemo: StrictMode (dev) chạy
 * cleanup → effect lần hai, nên URL tạo bằng useMemo sẽ bị revoke ở cleanup
 * đầu mà vẫn được dùng tiếp → ảnh/video/tiếng xem trước trắng trơn.
 */
export default function useObjectUrl(blob) {
  const [url, setUrl] = useState(null);
  useEffect(() => {
    if (!blob) { setUrl(null); return undefined; }
    const u = URL.createObjectURL(blob);
    setUrl(u);
    return () => URL.revokeObjectURL(u);
  }, [blob]);
  return url;
}
