import { useEffect, useState } from 'react';

/**
 * blob → object URL, revoked when the blob changes or the component unmounts.
 *
 * The URL is created INSIDE the effect, not in useMemo: StrictMode (dev) runs
 * cleanup → effect twice, so a useMemo URL would be revoked by the first
 * cleanup yet still used → blank image/video/audio previews.
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
