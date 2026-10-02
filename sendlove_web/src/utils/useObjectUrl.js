import { useEffect, useState } from 'react';

/**
 * blob → object URL, revoked on change or unmount. Created INSIDE the effect,
 * not in useMemo: StrictMode's double cleanup would revoke a memoized URL.
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
