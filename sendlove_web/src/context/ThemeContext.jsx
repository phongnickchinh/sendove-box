import React, { createContext, useContext, useEffect, useState } from 'react';

const ThemeContext = createContext();

export const useTheme = () => useContext(ThemeContext);

const STORAGE_KEY = 'sendlove-theme';

function readStored() {
  try {
    const v = localStorage.getItem(STORAGE_KEY);
    return v === 'dark' || v === 'light' ? v : 'light';
  } catch {
    return 'light';
  }
}

/**
 * Theme sáng/tối thật: đặt data-theme trên <html> để toàn bộ token bề mặt
 * trong tokens.css đảo theo (xem khối :root[data-theme='dark']). Mặc định
 * sáng — dự án chưa có nhu cầu đọc prefers-color-scheme của hệ điều hành.
 */
export function ThemeProvider({ children }) {
  const [theme, setTheme] = useState(readStored);

  useEffect(() => {
    document.documentElement.dataset.theme = theme;
    try { localStorage.setItem(STORAGE_KEY, theme); } catch { /* trình duyệt chặn storage, bỏ qua */ }
    // Thanh trạng thái của app đã cài (PWA) tô theo <meta name="theme-color">:
    // cho nó đi theo nền trang, không thì giao diện tối đội một thanh màu kem.
    const pageBg = getComputedStyle(document.documentElement).getPropertyValue('--bg-page').trim();
    if (pageBg) document.querySelector('meta[name="theme-color"]')?.setAttribute('content', pageBg);
  }, [theme]);

  const toggleTheme = () => setTheme((t) => (t === 'dark' ? 'light' : 'dark'));

  return (
    <ThemeContext.Provider value={{ theme, toggleTheme }}>
      {children}
    </ThemeContext.Provider>
  );
}
