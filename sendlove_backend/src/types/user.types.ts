import { BaseModel } from './base.types';

// ==================================================
// User — Node: users/{uid}
// ==================================================
export interface UserBoxEntry {
  role: 'sender' | 'receiver';
  box_name: string;
}

export interface User extends BaseModel {
  email: string;
  display_name: string;
  is_admin: boolean;
  avatar_url: string | null;
  last_login_at: number;
  is_deleted: boolean;

  /**
   * Denormalized copy: danh sách box mà user được pairing.
   * Key = box_id, Value = { role, box_name }
   */
  boxes_list: Record<string, UserBoxEntry>;
}
