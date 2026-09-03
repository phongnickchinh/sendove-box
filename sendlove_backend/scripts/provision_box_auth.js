#!/usr/bin/env node
/**
 * Tạo tài khoản Firebase Auth riêng cho MỘT box, với `uid` đặt đúng bằng BOX_ID.
 *
 * Vì sao uid phải trùng BOX_ID: `database.rules.json` cấp quyền bằng biểu thức
 * `auth.uid === $box_id`. Đặt uid trùng BOX_ID thì không cần bảng tra cứu
 * uid -> box nào cả, rule đọc thẳng được.
 *
 * Chạy OFFLINE lúc sản xuất/nạp firmware, KHÔNG phải dịch vụ chạy nền — box
 * không bao giờ gọi tới script này. Đây chính là lý do hướng "direct RTDB"
 * không cần backend lúc runtime.
 *
 * Yêu cầu trước khi chạy:
 *   1. Firebase Console > Authentication > Sign-in method > bật **Email/Password**
 *      (hiện đang tắt: REST trả về PASSWORD_LOGIN_DISABLED).
 *   2. `sendlove_backend/serviceAccountKey.json` tồn tại (đã có sẵn, đã gitignore).
 *
 * Dùng:
 *   node scripts/provision_box_auth.js ESP32_A1B2C3D4E5F6
 *   node scripts/provision_box_auth.js ESP32_A1B2C3D4E5F6 --reset-password
 *
 * In ra đoạn C++ để dán vào `sendlove_firmware/include/config_secrets.h`.
 * Mật khẩu chỉ hiện ĐÚNG MỘT LẦN lúc tạo — Firebase không cho đọc lại.
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

// Firebase Auth bắt buộc định dạng email hợp lệ nhưng không kiểm tra gửi được hay
// không. `.invalid` là TLD dành riêng theo RFC 2606 — chắc chắn không bao giờ
// trùng domain thật của ai.
const email = `${boxId.toLowerCase()}@box.sendlove.invalid`;

// 32 hex = 128 bit. Mật khẩu này không bao giờ người dùng gõ tay, nên dài tuỳ ý.
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
