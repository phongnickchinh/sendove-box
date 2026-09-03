import React from 'react';
import { HashRouter as Router, Routes, Route, Navigate } from 'react-router-dom';
import Login from './pages/Login';
import Dashboard from './pages/Dashboard';
import PairBox from './pages/PairBox';
import SenderUI from './pages/SenderUI';
import SenderDashboard from './pages/SenderDashboard';
import ReceiverUI from './pages/ReceiverUI';
import ReceiverAlarms from './pages/ReceiverAlarms';
import ReceiverConfig from './pages/ReceiverConfig';
import AuthRoute from './components/AuthRoute';

function App() {
  return (
    <Router>
      <Routes>
        {/* Trang Login công khai */}
        <Route path="/" element={<Login />} />

        {/* Mọi trang trong đây tự dựng <Screen> của mình — AuthRoute chỉ chặn khách. */}
        <Route element={<AuthRoute />}>
          <Route path="/dashboard" element={<Dashboard />} />
          <Route path="/pair" element={<PairBox />} />

          <Route path="/box/:boxId/sender" element={<SenderUI />} />
          {/* SenderUI.jsx điều hướng tới route này sau khi gửi xong — trước đây
              chưa khai báo nên bấm xong là rơi về trang chủ. */}
          <Route path="/box/:boxId/sender/dashboard" element={<SenderDashboard />} />

          <Route path="/box/:boxId/receiver" element={<ReceiverUI />} />
          <Route path="/box/:boxId/receiver/alarm" element={<ReceiverAlarms />} />
          <Route path="/box/:boxId/receiver/config" element={<ReceiverConfig />} />
        </Route>

        {/* Bắt mọi path sai về trang chủ */}
        <Route path="*" element={<Navigate to="/" replace />} />
      </Routes>
    </Router>
  );
}

export default App;
