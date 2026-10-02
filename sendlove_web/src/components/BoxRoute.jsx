import { Navigate, Outlet, useParams } from 'react-router-dom';
import { useAuth } from '../context/AuthContext';

/**
 * Guards a box's pages: a box no longer in the account → Dashboard; the wrong
 * role → the page for the actual role. If the profile failed to load (offline),
 * do NOT block: let the page report the network error itself.
 */
export default function BoxRoute({ role }) {
  const { boxId } = useParams();
  const { profile } = useAuth();

  if (!profile) return <Outlet />;

  const entry = profile.boxes_list?.[boxId];
  if (!entry) return <Navigate to="/dashboard" replace />;

  const actual = entry.role === 'sender' ? 'sender' : 'receiver';
  if (actual !== role) return <Navigate to={`/box/${boxId}/${actual}`} replace />;

  return <Outlet />;
}
