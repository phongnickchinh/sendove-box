import { Navigate, Outlet } from 'react-router-dom';
import { useAuth } from '../context/AuthContext';
import { Screen, Body } from './ui/Screen';

/**
 * Chặn khách chưa đăng nhập. Không còn dựng khung chung nữa: mọi trang tự dựng
 * <Screen> của mình (cột 430 theo thiết kế), nên Navbar và khung <main> 1200px
 * của bản cũ đã bị gỡ — hai paradigm layout song song là thứ làm giao diện lệch.
 * Đăng xuất giờ nằm ở AppBar của Dashboard.
 */
export default function AuthRoute() {
  const { user, loading } = useAuth();

  if (loading) {
    return (
      <Screen>
        <Body center>
          <span className="sl-heading">Đang tải…</span>
        </Body>
      </Screen>
    );
  }

  if (!user) return <Navigate to="/" replace />;

  return <Outlet />;
}
