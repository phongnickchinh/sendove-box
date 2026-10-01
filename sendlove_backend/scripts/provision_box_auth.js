#!/usr/bin/env node
/**
 * Creates a dedicated Firebase Auth account for ONE box, with `uid` equal to BOX_ID.
 *
 * Why uid must equal BOX_ID: `database.rules.json` grants access with
 * `auth.uid === $box_id`, so no uid -> box lookup table is needed.
 *
 * Run it OFFLINE at manufacturing/flashing time; it is NOT a background
 * service — the box never calls it. That is why the "direct RTDB" approach
 * needs no backend at runtime.
 *
 * Prerequisites:
 *   1. Firebase Console > Authentication > Sign-in method > enable **Email/Password**
 *      (while disabled, REST returns PASSWORD_LOGIN_DISABLED).
 *   2. `sendlove_backend/serviceAccountKey.json` exists (gitignored).
 *
 * Usage:
 *   node scripts/provision_box_auth.js ESP32_A1B2C3D4E5F6
 *   node scripts/provision_box_auth.js ESP32_A1B2C3D4E5F6 --reset-password
 *
 * Prints the C++ snippet to paste into `sendlove_firmware/include/config_secrets.h`.
 * The password is shown EXACTLY ONCE, at creation — Firebase can't read it back.
 */

const admin = require('firebase-admin');
const crypto = require('crypto');
const path = require('path');

const boxId = process.argv[2];
const resetPassword = process.argv.includes('--reset-password');

if (!boxId) {
  console.error('Thiếu BOX_ID.\n  node scripts/provision_box_auth.js <BOX_ID> [--reset-password]');
  process.exit(1);
}

// Firebase Auth requires a well-formed email but never checks deliverability.
// `.invalid` is a reserved TLD (RFC 2606) — it can never collide with a real domain.
const email = `${boxId.toLowerCase()}@box.sendlove.invalid`;

// 32 hex chars = 128 bits. Nobody ever types this password, so length is free.
const password = crypto.randomBytes(16).toString('hex');

const serviceAccount = require(path.join(__dirname, '..', 'serviceAccountKey.json'));
admin.initializeApp({ credential: admin.credential.cert(serviceAccount) });

(async () => {
  let created = false;
  try {
    await admin.auth().getUser(boxId);
    if (!resetPassword) {
      console.error(
        `\nBox "${boxId}" ĐÃ có tài khoản Auth rồi.\n` +
        `Không đổi gì cả (đổi mật khẩu sẽ làm mọi box đang chạy với mật khẩu cũ mất kết nối).\n` +
        `Nếu thật sự muốn cấp mật khẩu mới, chạy lại kèm --reset-password.\n`
      );
      process.exit(2);
    }
    await admin.auth().updateUser(boxId, { email, password });
  } catch (err) {
    if (err.code !== 'auth/user-not-found') throw err;
    await admin.auth().createUser({ uid: boxId, email, password });
    created = true;
  }

  console.log(`\n${created ? 'Đã tạo' : 'Đã đổi mật khẩu'} tài khoản Auth cho box.`);
  console.log(`  uid   = ${boxId}   (phải trùng BOX_ID để rule auth.uid === $box_id khớp)`);
  console.log(`  email = ${email}`);
  console.log('\nDán vào sendlove_firmware/include/config_secrets.h:\n');
  console.log(`static constexpr const char *BOX_AUTH_EMAIL    = "${email}";`);
  console.log(`static constexpr const char *BOX_AUTH_PASSWORD = "${password}";`);
  console.log('\nMật khẩu chỉ hiện MỘT LẦN — Firebase không cho đọc lại. Mất thì phải --reset-password.\n');
  process.exit(0);
})().catch((err) => {
  console.error('\nLỗi:', err.message || err);
  process.exit(1);
});
