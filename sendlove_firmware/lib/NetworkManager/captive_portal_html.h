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
    .rescan {
      float: right; background: none; border: none; color: #ff4081;
      font-size: 13px; cursor: pointer; padding: 0;
    }
    .netlist {
      max-height: 186px; overflow-y: auto; text-align: left;
      background: #121214; border: 1px solid #29292e; border-radius: 8px;
    }
    .netlist .msg { padding: 12px; color: #a8a8b3; font-size: 13px; }
    .net {
      display: flex; align-items: center; gap: 8px; width: 100%;
      padding: 11px 12px; background: none; border: none;
      border-bottom: 1px solid #29292e; color: #e1e1e6;
      font-size: 14px; text-align: left; cursor: pointer;
    }
    .net:last-child { border-bottom: none; }
    .net:hover, .net.sel { background: #29292e; }
    .net .name { flex: 1; overflow: hidden; text-overflow: ellipsis; white-space: nowrap; }
    .net .meta { color: #a8a8b3; font-size: 12px; }
  </style>
</head>
<body>
  <div class="card">
    <div class="logo">❤️ Sendlove Box</div>
    <div class="subtitle">Cấu hình kết nối Wi-Fi cho thiết bị</div>
    <form action="/save" method="POST">
      <div class="input-group">
        <label>Mạng xung quanh
          <button type="button" class="rescan" id="rescan">Quét lại</button>
        </label>
        <div class="netlist" id="netlist"><div class="msg">Đang quét…</div></div>
      </div>
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
    // Poll /scan: box quet bat dong bo nen lan dau tra "scanning", phai hoi lai.
    // Dung han khi "done" — khong duoc poll vo han vi moi vong lai kich mot lan
    // quet moi, lam nghen chinh cai AP nguoi dung dang nối vao.
    var pollLeft = 0;

    function setMsg(t) {
      var box = document.getElementById("netlist");
      box.innerHTML = "";
      var d = document.createElement("div");
      d.className = "msg";
      d.textContent = t;
      box.appendChild(d);
    }

    function bars(r) { return r >= -60 ? "▂▄▆" : (r >= -75 ? "▂▄" : "▂"); }

    function render(nets) {
      var box = document.getElementById("netlist");
      box.innerHTML = "";
      if (!nets.length) { setMsg("Không thấy mạng nào. Bấm Quét lại."); return; }
      nets.sort(function (a, b) { return b.rssi - a.rssi; });
      var seen = {};
      nets.forEach(function (n) {
        if (seen[n.ssid]) return;   // cung mot ten phat tu nhieu AP
        seen[n.ssid] = 1;
        var b = document.createElement("button");
        b.type = "button";
        b.className = "net";
        var nm = document.createElement("span");
        nm.className = "name";
        nm.textContent = n.ssid;    // textContent: ten mang la khong pha duoc trang
        var mt = document.createElement("span");
        mt.className = "meta";
        mt.textContent = (n.lock ? "🔒 " : "") + bars(n.rssi);
        b.appendChild(nm);
        b.appendChild(mt);
        b.onclick = function () {
          document.getElementById("ssid").value = n.ssid;
          var all = box.getElementsByClassName("net");
          for (var i = 0; i < all.length; i++) all[i].classList.remove("sel");
          b.classList.add("sel");
          document.getElementById("password").focus();
        };
        box.appendChild(b);
      });
    }

    function poll() {
      fetch("/scan").then(function (r) { return r.json(); }).then(function (d) {
        if (d.status === "done") { render(d.nets); return; }
        if (pollLeft-- > 0) setTimeout(poll, 1200);
        else setMsg("Quét lâu quá. Bấm Quét lại.");
      }).catch(function () { setMsg("Không đọc được danh sách. Bấm Quét lại."); });
    }

    function rescan() {
      pollLeft = 10;
      setMsg("Đang quét…");
      poll();
    }

    document.getElementById("rescan").onclick = rescan;
    rescan();

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
