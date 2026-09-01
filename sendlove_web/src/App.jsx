import React from 'react';
import { HashRouter as Router, Routes, Route, Navigate } from 'react-router-dom';
import Login from './pages/Login';
import Dashboard from './pages/Dashboard';
import PairBox from './pages/PairBox';
import SenderUI from './pages/SenderUI';
import ReceiverUI from './pages/ReceiverUI';
import AuthRoute from './components/AuthRoute';

function App() {
  return (
    <Router>
      <Routes>
        {/* Trang Login công khai */}
        <Route path="/" element={<Login />} />
        
        {/* Các trang yêu cầu đăng nhập */}
        <Route element={<AuthRoute />}>
          <Route path="/dashboard" element={<Dashboard />} />
          <Route path="/pair" element={<PairBox />} />
          <Route path="/box/:boxId/receiver" element={<ReceiverUI />} />
        </Route>

        {/* Luồng gửi dựng nguyên màn hình theo thiết kế mới nên không dùng Navbar */}
        <Route element={<AuthRoute bare />}>
          <Route path="/box/:boxId/sender" element={<SenderUI />} />
        </Route>

        {/* Bắt mọi path sai về trang chủ */}
        <Route path="*" element={<Navigate to="/" replace />} />
      </Routes>
    </Router>
  );
}

export default App;
