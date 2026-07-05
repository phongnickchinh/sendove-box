import { Request, Response, NextFunction } from 'express';
import { ApiResponse } from '../types/api.types';

export const errorHandler = (err: any, req: Request, res: Response<ApiResponse>, next: NextFunction) => {
  console.error('[Error Handler]', err);

  const statusCode = err.statusCode || 500;
  const message = err.message || 'Internal Server Error';
  const code = err.code || 'internal_error';

  res.status(statusCode).json({
    success: false,
    error: {
      code,
      message,
    }
  });
};

export class AppError extends Error {
  statusCode: number;
  code: string;

  constructor(statusCode: number, code: string, message: string) {
    super(message);
    this.statusCode = statusCode;
    this.code = code;
  }
}
