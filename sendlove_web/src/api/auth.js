import { signInWithPopup, signInWithRedirect, getRedirectResult, signOut } from "firebase/auth";
import { auth, googleProvider, facebookProvider } from "../config/firebase";

/**
 * Popup bị trình duyệt di động chặn / không hỗ trợ → chuyển sang redirect.
 * Kết quả redirect KHÔNG trả về đây: trang tải lại, onAuthStateChanged trong
 * AuthContext nhận user như mọi lần đăng nhập khác.
 *
 * Lưu ý cấu hình: trên Chrome/Safari mới (chặn lưu trữ bên thứ ba), redirect
 * chỉ chạy ổn khi VITE_FIREBASE_AUTH_DOMAIN là CHÍNH tên miền đang host web
 * (iot-app-839a2.web.app), không phải *.firebaseapp.com.
 */
const REDIRECT_INSTEAD = new Set([
  'auth/popup-blocked',
  'auth/cancelled-popup-request',
  'auth/operation-not-supported-in-this-environment',
]);

async function signInWith(provider, label) {
  try {
    const result = await signInWithPopup(auth, provider);
    return { user: result.user, error: null };
  } catch (error) {
    if (REDIRECT_INSTEAD.has(error?.code)) {
      try {
        await signInWithRedirect(auth, provider);
        return { user: null, error: null, redirecting: true };
      } catch (redirectError) {
        console.error(`Lỗi đăng nhập ${label} (redirect):`, redirectError);
        return { user: null, error: redirectError };
      }
    }
    // Người dùng tự đóng cửa sổ: không phải lỗi, chỉ trả nút về như cũ.
    if (error?.code === 'auth/popup-closed-by-user') {
      return { user: null, error: null, cancelled: true };
    }
    console.error(`Lỗi đăng nhập ${label}:`, error);
    return { user: null, error };
  }
}

/** Đăng nhập bằng tài khoản Google (popup, dự phòng redirect). */
export const signInWithGoogle = () => signInWith(googleProvider, 'Google');

/**
 * Đăng nhập bằng tài khoản Facebook (popup, dự phòng redirect). Nếu Facebook
 * chưa được bật trong Firebase Console, Firebase trả lỗi
 * "auth/operation-not-allowed" — hiện đúng thông báo lỗi, không crash.
 */
export const signInWithFacebook = () => signInWith(facebookProvider, 'Facebook');

/**
 * Lỗi của lượt redirect vừa quay về (nếu có). Thành công thì không cần gọi —
 * onAuthStateChanged đã nhận user; hàm này chỉ để hiện lỗi thay vì im lặng.
 */
export const readRedirectError = async () => {
  try {
    await getRedirectResult(auth);
    return null;
  } catch (error) {
    console.error("Lỗi đăng nhập (redirect):", error);
    return error;
  }
};

/**
 * Đăng xuất
 */
export const logOut = async () => {
  try {
    await signOut(auth);
    return { success: true, error: null };
  } catch (error) {
    console.error("Lỗi đăng xuất:", error);
    return { success: false, error };
  }
};
