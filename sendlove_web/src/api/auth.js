import { signInWithPopup, signOut } from "firebase/auth";
import { auth, googleProvider, facebookProvider } from "../config/firebase";

/**
 * Đăng nhập bằng tài khoản Google (Popup)
 */
export const signInWithGoogle = async () => {
  try {
    const result = await signInWithPopup(auth, googleProvider);
    return { user: result.user, error: null };
  } catch (error) {
    console.error("Lỗi đăng nhập Google:", error);
    return { user: null, error };
  }
};

/**
 * Đăng nhập bằng tài khoản Facebook (Popup). Nếu Facebook chưa được bật
 * trong Firebase Console, Firebase trả lỗi "auth/operation-not-allowed" —
 * hiện đúng thông báo lỗi, không crash.
 */
export const signInWithFacebook = async () => {
  try {
    const result = await signInWithPopup(auth, facebookProvider);
    return { user: result.user, error: null };
  } catch (error) {
    console.error("Lỗi đăng nhập Facebook:", error);
    return { user: null, error };
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
