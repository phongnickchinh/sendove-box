import * as admin from 'firebase-admin';
import * as fs from 'fs';
import * as path from 'path';

if (!admin.apps.length) {
  const config: admin.AppOptions = {
    databaseURL: 'https://iot-app-839a2.asia-southeast1.firebasedatabase.app',
    storageBucket: 'iot-app-839a2.firebasestorage.app'
  };

  // Explicitly load the service account to bypass Firebase Emulator's ADC
  const serviceAccountPath = path.resolve(__dirname, '../serviceAccountKey.json');
  if (fs.existsSync(serviceAccountPath)) {
    const serviceAccount = require(serviceAccountPath);
    config.credential = admin.credential.cert(serviceAccount);
  }

  admin.initializeApp(config);
}

export const db = admin.database();
export const storage = admin.storage();
