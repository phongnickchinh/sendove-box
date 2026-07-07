import React, { createContext, useContext, useEffect, useState } from 'react';
import { onAuthStateChanged } from 'firebase/auth';
import { auth } from '../config/firebase';
import apiClient from '../api/client';

const AuthContext = createContext();

export const useAuth = () => useContext(AuthContext);

export const AuthProvider = ({ children }) => {
  const [user, setUser] = useState(null);
  const [profile, setProfile] = useState(null); // Data from backend (e.g. boxes_list)
  const [loading, setLoading] = useState(true);

  // Lấy dữ liệu profile từ backend sau khi có auth token
  const fetchProfile = async () => {
    try {
      const res = await apiClient.get('/users/me');
      if (res.data.success) {
        setProfile(res.data.data);
      }
    } catch (error) {
      console.error("Failed to fetch user profile:", error);
      // Nếu user chưa tồn tại trên backend (vừa đăng ký xong), backend middleware 'requireAuth'
      // tự động gọi userRepository.createOrUpdate để tạo user mới nên thường sẽ không lỗi 404,
      // nhưng cứ an toàn catch lỗi ở đây.
    }
  };

  useEffect(() => {
    const unsubscribe = onAuthStateChanged(auth, async (currentUser) => {
      setUser(currentUser);
      if (currentUser) {
        await fetchProfile();
      } else {
        setProfile(null);
      }
      setLoading(false);
    });

    return unsubscribe;
  }, []);

  const value = {
    user,
    profile,
    loading,
    refreshProfile: fetchProfile // Export để component khác gọi lại khi thêm box mới
  };

  return (
    <AuthContext.Provider value={value}>
      {!loading && children}
    </AuthContext.Provider>
  );
};
