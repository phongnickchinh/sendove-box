import { Navigate, Outlet, useParams } from 'react-router-dom';
import { useAuth } from '../context/AuthContext';

/**
 * Guards a box's pages when:
 *   - the box is no longer in the account (unpaired, or a stale link)
 *     → go to the Dashboard;
 *   - the role is wrong (a sender typing a /receiver/... URL or vice versa)
 *     → go to the page for the actual role.
 * Without it the pages would call the API and get a string of raw 403s.
 *
 * If the profile failed to load (offline), do NOT block: the box list is
 * unknown, so don't guess — let the page report the network error itself.
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
