import { Request } from 'express';

export interface ApiResponse<T = any> {
  success: boolean;
  data?: T;
  error?: {
    code: string;
    message: string;
  };
}

// User Request type (with UID attached by middleware)
export interface AuthenticatedRequest extends Request {
  user?: {
    uid: string;
    email?: string;
    name?: string;
    picture?: string;
  };
  deviceId?: string; // For ESP32 requests
}
