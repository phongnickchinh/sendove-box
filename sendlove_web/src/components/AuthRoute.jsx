import { Navigate, Outlet } from 'react-router-dom';
import { useAuth } from '../context/AuthContext';
import { Screen, Body } from './ui/Screen';

/**
 * Blocks signed-out visitors. It renders no shared frame: every page builds its
 * own <Screen> (the 430 column from the design). Sign-out lives in the
 * Dashboard's AppBar.
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
