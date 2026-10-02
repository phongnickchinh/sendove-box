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
 * Light/dark theme: sets data-theme on <html> (see tokens.css). Defaults to
 * light — the OS prefers-color-scheme is intentionally not read.
 */
export function ThemeProvider({ children }) {
  const [theme, setTheme] = useState(readStored);

  useEffect(() => {
    document.documentElement.dataset.theme = theme;
    try { localStorage.setItem(STORAGE_KEY, theme); } catch { /* storage blocked by the browser; ignore */ }
    // The installed app's (PWA) status bar is tinted by <meta name="theme-color">:
    // keep it in sync with the page background, or dark mode wears a cream bar.
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
