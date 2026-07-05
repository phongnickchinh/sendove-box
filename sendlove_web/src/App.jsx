import React from 'react';
import { BrowserRouter as Router, Routes, Route } from 'react-router-dom';
import Login from './pages/Login';
import Home from './pages/Home';
import SenderDashboard from './pages/SenderDashboard';
import ReceiverDashboard from './pages/ReceiverDashboard';

function App() {
  return (
    <Router>
      <div className="app-container">
        <Routes>
          <Route path="/" element={<Login />} />
          <Route path="/home" element={<Home />} />
          <Route path="/send" element={<SenderDashboard />} />
          <Route path="/manage" element={<ReceiverDashboard />} />
        </Routes>
      </div>
    </Router>
  );
}

export default App;
