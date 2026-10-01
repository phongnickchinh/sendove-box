import { signInWithPopup, signInWithRedirect, getRedirectResult, signOut } from "firebase/auth";
import { auth, googleProvider, facebookProvider } from "../config/firebase";

/**
 * Popup blocked / unsupported by a mobile browser → fall back to redirect.
 * The redirect result does NOT come back here: the page reloads and
 * onAuthStateChanged in AuthContext receives the user like any other sign-in.
 *
 * Config note: on current Chrome/Safari (third-party storage blocked), redirect
 * only works when VITE_FIREBASE_AUTH_DOMAIN is the SAME domain that hosts the
 * web app (iot-app-839a2.web.app), not *.firebaseapp.com.
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
    // The user closed the popup: not an error, just reset the button.
    if (error?.code === 'auth/popup-closed-by-user') {
      return { user: null, error: null, cancelled: true };
    }
    console.error(`Lỗi đăng nhập ${label}:`, error);
    return { user: null, error };
  }
}

/** Sign in with Google (popup, redirect fallback). */
export const signInWithGoogle = () => signInWith(googleProvider, 'Google');

/**
 * Sign in with Facebook (popup, redirect fallback). If Facebook isn't enabled
 * in the Firebase Console, Firebase returns "auth/operation-not-allowed" —
 * shown as a proper error message, no crash.
 */
export const signInWithFacebook = () => signInWith(facebookProvider, 'Facebook');

/**
 * The error of the redirect that just returned, if any. Success needs no call —
 * onAuthStateChanged already has the user; this only surfaces the error
 * instead of failing silently.
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

/** Sign out. */
export const logOut = async () => {
  try {
    await signOut(auth);
    return { success: true, error: null };
  } catch (error) {
    console.error("Lỗi đăng xuất:", error);
    return { success: false, error };
  }
};
