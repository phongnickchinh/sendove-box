#ifndef CAPTIVE_PORTAL_HTML_H
#define CAPTIVE_PORTAL_HTML_H

#include <pgmspace.h>

// ============================================================================
// Trang HTML cho SoftAP Captive Portal Wi-Fi Setup
// ============================================================================
// Lưu trữ trong Flash memory (PROGMEM) để tiết kiệm RAM.
// ============================================================================

const char CAPTIVE_PORTAL_HTML[] PROGMEM = R"raw(
<!DOCTYPE html>
<html lang="vi">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Sendlove Box - Wi-Fi Setup</title>
  <style>
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body {
      font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif;
      background: #121214;
      color: #e1e1e6;
      display: flex;
      justify-content: center;
      align-items: center;
      min-height: 100vh;
      padding: 20px;
    }
    .card {
      background: #202024;
      border-radius: 16px;
      padding: 32px 24px;
      width: 100%;
      max-width: 360px;
      box-shadow: 0 10px 25px rgba(0,0,0,0.5);
      text-align: center;
      border: 1px solid #29292e;
    }
    .logo {
      font-size: 24px;
      font-weight: bold;
      color: #ff4081;
      margin-bottom: 8px;
    }
    .subtitle {
      font-size: 14px;
      color: #a8a8b3;
      margin-bottom: 24px;
    }
    .input-group {
      margin-bottom: 16px;
      text-align: left;
    }
    label {
      display: block;
      font-size: 12px;
      color: #a8a8b3;
      margin-bottom: 6px;
      text-transform: uppercase;
      letter-spacing: 0.5px;
    }
    input[type=text], input[type=password] {
      width: 100%;
      padding: 12px 14px;
      background: #121214;
      border: 1px solid #323238;
      border-radius: 8px;
      color: #fff;
      font-size: 15px;
      outline: none;
      transition: border-color 0.2s;
    }
    input[type=text]:focus, input[type=password]:focus {
      border-color: #ff4081;
    }
    input[type=submit] {
      width: 100%;
      background: linear-gradient(90deg, #ff4081, #f50057);
      color: #fff;
      border: none;
      padding: 14px;
      border-radius: 8px;
      font-size: 16px;
      font-weight: bold;
      cursor: pointer;
      margin-top: 12px;
      box-shadow: 0 4px 12px rgba(255, 64, 129, 0.3);
      transition: opacity 0.2s;
    }
    input[type=submit]:hover {
      opacity: 0.9;
    }
  </style>
</head>
<body>
  <div class="card">
    <div class="logo">❤️ Sendlove Box</div>
    <div class="subtitle">Cấu hình kết nối Wi-Fi cho thiết bị</div>
    <form action="/save" method="POST">
      <div class="input-group">
        <label for="ssid">Tên Wi-Fi (SSID)</label>
        <input type="text" id="ssid" name="ssid" placeholder="Nhập tên mạng Wi-Fi" required autocomplete="off">
      </div>
      <div class="input-group">
        <label for="password">Mật khẩu Wi-Fi</label>
        <div style="position: relative; display: flex; align-items: center;">
          <input type="password" id="password" name="password" placeholder="Nhập mật khẩu Wi-Fi" style="padding-right: 40px;">
          <button type="button" onclick="togglePass()" style="position: absolute; right: 10px; background: none; border: none; color: #a8a8b3; cursor: pointer; font-size: 16px; outline: none;">👁️</button>
        </div>
      </div>
      <input type="submit" value="LƯU & KẾT NỐI">
    </form>
  </div>
  <script>
    function togglePass() {
      var p = document.getElementById("password");
      if (p.type === "password") { p.type = "text"; }
      else { p.type = "password"; }
    }
  </script>
</body>
</html>
)raw";

#endif // CAPTIVE_PORTAL_HTML_H
