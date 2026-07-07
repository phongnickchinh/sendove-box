import { Navigate, Outlet } from 'react-router-dom';
import { useAuth } from '../context/AuthContext';
import Navbar from './Navbar';

export default function AuthRoute() {
  const { user, loading } = useAuth();

  if (loading) {
    return (
      <div style={{ display: 'flex', height: '100vh', justifyContent: 'center', alignItems: 'center' }}>
        <div className="text-gradient" style={{ fontSize: '1.2rem', fontWeight: 600 }}>Đang tải...</div>
      </div>
    );
  }

  // Nếu chưa đăng nhập, đá về trang Login
  if (!user) {
    return <Navigate to="/" replace />;
  }

  // Nếu đã đăng nhập, render Navbar và các child routes (Outlet)
  return (
    <div className="fade-in" style={{ display: 'flex', flexDirection: 'column', minHeight: '100vh' }}>
      <Navbar user={user} />
      <main style={{ flex: 1, padding: '24px', maxWidth: '1200px', margin: '0 auto', width: '100%' }}>
        <Outlet />
      </main>
    </div>
  );
}
