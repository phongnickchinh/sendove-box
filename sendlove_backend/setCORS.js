const admin = require('firebase-admin');
const serviceAccount = require('./serviceAccountKey.json');

admin.initializeApp({
  credential: admin.credential.cert(serviceAccount),
  storageBucket: 'iot-app-839a2.firebasestorage.app' // replace if the real bucket differs
});

async function setCors() {
  const bucket = admin.storage().bucket();
  const corsConfig = [
    {
      origin: ['*'], // restrict origins for production (e.g. the hosting domain)
      method: ['GET', 'PUT', 'POST', 'DELETE', 'OPTIONS'],
      responseHeader: ['Content-Type', 'Authorization', 'x-goog-meta-*'],
      maxAgeSeconds: 3600
    }
  ];

  try {
    await bucket.setCorsConfiguration(corsConfig);
    console.log('Successfully set CORS for bucket:', bucket.name);
  } catch (error) {
    console.error('Error setting CORS:', error);
  }
}

setCors();
