import * as admin from 'firebase-admin';

if (!admin.apps.length) {
  admin.initializeApp({
    databaseURL: 'https://iot-app-839a2.asia-southeast1.firebasedatabase.app'
  });
}

export const db = admin.database();
export const storage = admin.storage();
