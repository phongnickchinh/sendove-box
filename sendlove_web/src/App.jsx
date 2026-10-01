import React, { lazy, Suspense, useEffect } from 'react';
import { Screen, Body } from './components/ui/Screen';
import { HashRouter as Router, Routes, Route, Navigate, useLocation } from 'react-router-dom';
import Login from './pages/Login';
import Dashboard from './pages/Dashboard';
import PairBox from './pages/PairBox';
import SenderUI from './pages/SenderUI';
import SenderDashboard from './pages/SenderDashboard';
import ReceiverUI from './pages/ReceiverUI';
import ReceiverAlarms from './pages/ReceiverAlarms';
import ReceiverConfig from './pages/ReceiverConfig';
// Receiver-only, rarely opened pages — split into their own lazy chunks.
const ThemePicker = lazy(() => import('./pages/ThemePicker'));
const ThemeEditor = lazy(() => import('./pages/ThemeEditor'));
const ThemeSend = lazy(() => import('./pages/ThemeSend'));
const ReceiverMusic = lazy(() => import('./pages/ReceiverMusic'));
import AuthRoute from './components/AuthRoute';
import BoxRoute from './components/BoxRoute';
import { SendProvider } from './context/SendContext';

/** An SPA doesn't scroll to the top on navigation: a new page would open at the previous page's scroll position. */
function ScrollToTop() {
  const { pathname } = useLocation();
  useEffect(() => { window.scrollTo(0, 0); }, [pathname]);
  return null;
}

function App() {
  return (
    <Router>
      <SendProvider>
      <ScrollToTop />
      <Suspense fallback={<Screen><Body center><span className="sl-heading">Đang tải…</span></Body></Screen>}>
      <Routes>
        {/* Public login page */}
        <Route path="/" element={<Login />} />

        {/* Every page in here renders its own <Screen> — AuthRoute only blocks guests. */}
        <Route element={<AuthRoute />}>
          <Route path="/dashboard" element={<Dashboard />} />
          <Route path="/pair" element={<PairBox />} />

          {/* BoxRoute: the box must still be in the account with the right role;
              otherwise redirect instead of letting the page hit a 403. */}
          <Route path="/box/:boxId/sender" element={<BoxRoute role="sender" />}>
            <Route index element={<SenderUI />} />
            {/* Sent-message history (opened from step 1). */}
            <Route path="dashboard" element={<SenderDashboard />} />
          </Route>

          <Route path="/box/:boxId/receiver" element={<BoxRoute role="receiver" />}>
            <Route index element={<ReceiverUI />} />
            <Route path="alarm" element={<ReceiverAlarms />} />
            <Route path="alarm/music" element={<ReceiverMusic />} />
            <Route path="config" element={<ReceiverConfig />} />
            {/* Standby-screen theme: saved through /boxes/:boxId/theme — see theme/layout.js. */}
            <Route path="theme" element={<ThemePicker />} />
            <Route path="theme/edit" element={<ThemeEditor />} />
            <Route path="theme/send" element={<ThemeSend />} />
          </Route>
        </Route>

        {/* Unknown paths go home */}
        <Route path="*" element={<Navigate to="/" replace />} />
      </Routes>
      </Suspense>
      </SendProvider>
    </Router>
  );
}

export default App;
