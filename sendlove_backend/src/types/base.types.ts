// Base model: fields every entity inherits
export interface BaseModel {
  id: string;
  created_at: number;
  updated_at: number;
  deleted_at?: number | null;
}
