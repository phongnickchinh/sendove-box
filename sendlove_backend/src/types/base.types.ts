// ==================================================
// Base Model: Mọi entity đều kế thừa các trường này
// ==================================================
export interface BaseModel {
  id: string;
  created_at: number;
  updated_at: number;
  deleted_at?: number | null;
}
