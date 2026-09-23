import { initializeApp } from "firebase/app";
import { getAuth, GoogleAuthProvider, FacebookAuthProvider } from "firebase/auth";

// Cấu hình Firebase từ biến môi trường Vite
const firebaseConfig = {
  apiKey: import.meta.env.VITE_FIREBASE_API_KEY,
  authDomain: import.meta.env.VITE_FIREBASE_AUTH_DOMAIN,
  databaseURL: import.meta.env.VITE_FIREBASE_DATABASE_URL,
  projectId: import.meta.env.VITE_FIREBASE_PROJECT_ID,
  storageBucket: import.meta.env.VITE_FIREBASE_STORAGE_BUCKET,
  messagingSenderId: import.meta.env.VITE_FIREBASE_MESSAGING_SENDER_ID,
  appId: import.meta.env.VITE_FIREBASE_APP_ID
};

// Khởi tạo Firebase App
const app = initializeApp(firebaseConfig);

// Khởi tạo các dịch vụ
export const auth = getAuth(app);
// Web không đọc RTDB hay Storage trực tiếp — mọi thứ đi qua backend (api/*),
// media dùng signed URL. Khởi tạo getDatabase/getStorage chỉ để thừa từng làm
// bundle nặng thêm ~123 kB (645 → 522 kB), nên đã bỏ.

// Provider cho Google Sign-in
export const googleProvider = new GoogleAuthProvider();

// Provider cho Facebook Sign-in. Phải tự bật "Facebook" trong
// Firebase Console > Authentication > Sign-in method (cần App ID/Secret từ
// Meta for Developers) — không cấu hình được từ code.
export const facebookProvider = new FacebookAuthProvider();

export default app;
