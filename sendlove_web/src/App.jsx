import React, { lazy, Suspense } from 'react';
import { Screen, Body } from './components/ui/Screen';
import { HashRouter as Router, Routes, Route, Navigate } from 'react-router-dom';
import Login from './pages/Login';
import Dashboard from './pages/Dashboard';
import PairBox from './pages/PairBox';
import SenderUI from './pages/SenderUI';
import SenderDashboard from './pages/SenderDashboard';
import ReceiverUI from './pages/ReceiverUI';
import ReceiverAlarms from './pages/ReceiverAlarms';
import ReceiverConfig from './pages/ReceiverConfig';
// Ba trang theme chỉ người nhận mở, và hiếm — tách chunk riêng (lazy).
const ThemePicker = lazy(() => import('./pages/ThemePicker'));
const ThemeEditor = lazy(() => import('./pages/ThemeEditor'));
const ThemeSend = lazy(() => import('./pages/ThemeSend'));
const ReceiverMusic = lazy(() => import('./pages/ReceiverMusic'));
import AuthRoute from './components/AuthRoute';
import BoxRoute from './components/BoxRoute';

function App() {
  return (
    <Router>
      <Suspense fallback={<Screen><Body center><span className="sl-heading">Đang tải…</span></Body></Screen>}>
      <Routes>
        {/* Trang Login công khai */}
        <Route path="/" element={<Login />} />

        {/* Mọi trang trong đây tự dựng <Screen> của mình — AuthRoute chỉ chặn khách. */}
        <Route element={<AuthRoute />}>
          <Route path="/dashboard" element={<Dashboard />} />
          <Route path="/pair" element={<PairBox />} />

          {/* BoxRoute: hộp phải còn trong tài khoản và đúng vai trò, không thì
              chuyển hướng thay vì để trang nhận 403. */}
          <Route path="/box/:boxId/sender" element={<BoxRoute role="sender" />}>
            <Route index element={<SenderUI />} />
            {/* Lịch sử tin đã gửi (nút "Lịch sử tin nhắn" ở bước 1). */}
            <Route path="dashboard" element={<SenderDashboard />} />
          </Route>

          <Route path="/box/:boxId/receiver" element={<BoxRoute role="receiver" />}>
            <Route index element={<ReceiverUI />} />
            <Route path="alarm" element={<ReceiverAlarms />} />
            <Route path="alarm/music" element={<ReceiverMusic />} />
            <Route path="config" element={<ReceiverConfig />} />
            {/* Theme màn chờ: lưu qua /boxes/:boxId/theme. Firmware chưa tải về — xem theme/layout.js. */}
            <Route path="theme" element={<ThemePicker />} />
            <Route path="theme/edit" element={<ThemeEditor />} />
            <Route path="theme/send" element={<ThemeSend />} />
          </Route>
        </Route>

        {/* Bắt mọi path sai về trang chủ */}
        <Route path="*" element={<Navigate to="/" replace />} />
      </Routes>
      </Suspense>
    </Router>
  );
}

export default App;
