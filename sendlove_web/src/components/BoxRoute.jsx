import { Navigate, Outlet, useParams } from 'react-router-dom';
import { useAuth } from '../context/AuthContext';

/**
 * Chặn vào trang của một hộp khi:
 *   - hộp không còn trong tài khoản (đã huỷ ghép, hoặc mở lại link cũ)
 *     → về Dashboard;
 *   - sai vai trò (người gửi gõ URL /receiver/... hoặc ngược lại)
 *     → sang đúng trang của vai trò thật.
 * Không có lớp này thì các trang tự gọi API và nhận một loạt lỗi 403 thô.
 *
 * Profile chưa tải được (mất mạng) thì KHÔNG chặn: không biết danh sách hộp
 * thì không được đoán; để trang tự báo lỗi mạng như bình thường.
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
