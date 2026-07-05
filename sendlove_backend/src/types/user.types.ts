export interface User {
  uid: string;
  displayName: string | null;
  email: string | null;
  photoURL: string | null;
  createdAt: number;
  pairedBoxes?: Record<string, 'sender' | 'receiver'>; // boxId -> role
}
