import axios from 'axios';
import { auth } from '../config/firebase';

const envUrl = import.meta.env.VITE_API_URL;
const baseURL = (envUrl ? envUrl.trim() : null) || 'http://127.0.0.1:5001/iot-app-839a2/us-central1/api';

const apiClient = axios.create({
  baseURL,
  headers: {
    'Content-Type': 'application/json',
  },
});

// Interceptor: Gắn token của Firebase Auth vào mỗi request
apiClient.interceptors.request.use(async (config) => {
  const user = auth.currentUser;
  if (user) {
    const token = await user.getIdToken();
    config.headers.Authorization = `Bearer ${token}`;
  }
  return config;
}, (error) => {
  return Promise.reject(error);
});

export default apiClient;
