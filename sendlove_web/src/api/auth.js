import { signInWithPopup, signOut } from "firebase/auth";
import { auth, googleProvider } from "../config/firebase";

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
