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
  // The profile failed to load (offline, backend down). Without this flag the
  // Dashboard shows "no boxes yet" and the user thinks their paired boxes are gone.
  const [profileError, setProfileError] = useState(false);

  // Fetch the backend profile once there is an auth token.
  const fetchProfile = async () => {
    try {
      const res = await apiClient.get('/users/me');
      if (res.data.success) {
        setProfile(res.data.data);
        setProfileError(false);
      }
    } catch (error) {
      console.error("Failed to fetch user profile:", error);
      setProfileError(true);
      // A brand-new user normally doesn't 404: the backend's requireAuth
      // middleware creates the user record on first request.
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
    profileError,
    loading,
    refreshProfile: fetchProfile // lets other components reload after pairing a new box
  };

  return (
    <AuthContext.Provider value={value}>
      {!loading && children}
    </AuthContext.Provider>
  );
};
