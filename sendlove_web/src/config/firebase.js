import { initializeApp } from "firebase/app";
import { getAuth, GoogleAuthProvider, FacebookAuthProvider } from "firebase/auth";

// Firebase config from Vite env variables.
const firebaseConfig = {
  apiKey: import.meta.env.VITE_FIREBASE_API_KEY,
  authDomain: import.meta.env.VITE_FIREBASE_AUTH_DOMAIN,
  databaseURL: import.meta.env.VITE_FIREBASE_DATABASE_URL,
  projectId: import.meta.env.VITE_FIREBASE_PROJECT_ID,
  storageBucket: import.meta.env.VITE_FIREBASE_STORAGE_BUCKET,
  messagingSenderId: import.meta.env.VITE_FIREBASE_MESSAGING_SENDER_ID,
  appId: import.meta.env.VITE_FIREBASE_APP_ID
};

const app = initializeApp(firebaseConfig);

export const auth = getAuth(app);
// The web never reads RTDB or Storage directly — everything goes through the
// backend (api/*) and media uses signed URLs. Don't add getDatabase/getStorage:
// unused, they cost ~123 kB of bundle.

// Provider for Google sign-in
export const googleProvider = new GoogleAuthProvider();

// Facebook sign-in provider. "Facebook" must be enabled by hand in Firebase
// Console > Authentication > Sign-in method (needs an App ID/Secret from Meta
// for Developers) — it can't be configured from code.
export const facebookProvider = new FacebookAuthProvider();

export default app;
