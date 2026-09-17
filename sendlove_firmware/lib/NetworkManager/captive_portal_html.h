#ifndef CAPTIVE_PORTAL_HTML_H
#define CAPTIVE_PORTAL_HTML_H

#include <pgmspace.h>

// ============================================================================
// Trang HTML cho SoftAP Captive Portal Wi-Fi Setup
// ============================================================================
// Luu trong Flash (PROGMEM) de tiet kiem RAM.
//
// Giao dien theo he thiet ke "Warm Minimalism" cua file Figma — cung bang mau,
// cung thang chu, cung ban kinh bo goc voi web (sendlove_web/src/styles).
// Khong dung web font: luc mo trang nay hop CHUA co Internet, font ngoai se
// tai hong va chu nhay sang font du phong.
//
// Phan <script> ben duoi GIU NGUYEN khong doi mot ky tu:
//   - vong poll /scan (scanning -> done, 10 lan, 1200ms)
//   - textContent khi in ten mang (SSID la chuoi khong tin duoc)
//   - hop dong POST /save voi hai field ssid / password
// Doi CSS thi khong sao; doi mach nay la co the lam hop khong len duoc mang.
// ============================================================================

const char CAPTIVE_PORTAL_HTML[] PROGMEM = R"raw(
<!DOCTYPE html>
<html lang="vi">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Sendlove Box — Kết nối Wi-Fi</title>
  <style>
    :root {
      --rose-50:#FDF0EF; --rose-300:#F4A3AF; --rose-400:#F28AA1; --rose-700:#9E3A52;
      --car-50:#FDF8F3; --car-300:#E7AE75; --car-700:#83513E; --car-800:#603A2C; --car-900:#3D2A20;
      --n-0:#FFFFFF; --n-100:#EAE0D6; --n-400:#A89689; --n-500:#8A7767; --n-700:#5C4A3E;
    }
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body {
      font-family: "Quicksand", "Segoe UI", system-ui, -apple-system, sans-serif;
      background: var(--car-50);
      color: var(--car-900);
      display: flex; justify-content: center;
      min-height: 100vh; padding: 24px 16px;
      -webkit-font-smoothing: antialiased;
    }
    .card {
      background: var(--n-0);
      border: 1px solid var(--car-300);
      border-radius: 20px;
      padding: 24px 20px;
      width: 100%; max-width: 390px;
      align-self: flex-start;
    }
    .logo { font-size: 22px; font-weight: 700; line-height: 1.2; color: var(--car-900); }
    .subtitle { font-size: 15px; line-height: 1.6; color: var(--n-500); margin: 4px 0 20px; }
    .input-group { margin-bottom: 16px; }
    label {
      display: block; font-size: 14px; font-weight: 500; line-height: 1.4;
      color: var(--n-700); margin-bottom: 6px;
    }
    input[type=text], input[type=password] {
      width: 100%; padding: 12px 16px;
      background: var(--n-0);
      border: 1px solid var(--car-300);
      border-radius: 8px;
      color: var(--car-900);
      font-family: inherit; font-size: 15px; line-height: 1.4;
      outline: none; transition: border-color .18s, box-shadow .18s;
    }
    input[type=text]::placeholder, input[type=password]::placeholder { color: var(--n-500); }
    input[type=text]:focus, input[type=password]:focus {
      border-color: var(--rose-400);
      box-shadow: 0 0 0 3px var(--rose-50);
    }
    /* Nut chinh: nen hong, chu caramel/900. Chu trang tren nen nay chi dat
       ~2:1 do tuong phan; caramel/900 dat ~7:1. */
    input[type=submit] {
      width: 100%; min-height: 44px;
      background: var(--rose-400);
      color: var(--car-900);
      border: none; border-radius: 8px;
      font-family: inherit; font-size: 14px; font-weight: 600; line-height: 1.4;
      cursor: pointer; margin-top: 8px;
      transition: filter .18s;
    }
    input[type=submit]:hover { filter: brightness(.96); }
    .rescan {
      float: right; background: none; border: none; color: var(--rose-700);
      font-family: inherit; font-size: 12px; font-weight: 600; cursor: pointer; padding: 0;
    }
    .netlist {
      max-height: 186px; overflow-y: auto; text-align: left;
      background: var(--n-0); border: 1px solid var(--car-300); border-radius: 8px;
    }
    .netlist .msg { padding: 12px; color: var(--n-500); font-size: 12px; line-height: 1.4; }
    .net {
      display: flex; align-items: center; gap: 8px; width: 100%;
      padding: 11px 12px; background: none; border: none;
      border-bottom: 1px solid var(--n-100); color: var(--car-900);
      font-family: inherit; font-size: 14px; font-weight: 500;
      text-align: left; cursor: pointer;
    }
    .net:last-child { border-bottom: none; }
    .net:hover { background: var(--car-50); }
    .net.sel { background: var(--rose-50); }
    .net .name { flex: 1; overflow: hidden; text-overflow: ellipsis; white-space: nowrap; }
    .net .meta { color: var(--n-500); font-size: 12px; }
    .eye {
      position: absolute; right: 6px; background: none; border: none;
      color: var(--n-500); cursor: pointer; font-size: 16px; padding: 6px;
      line-height: 1;
    }
    .tips {
      display: flex; gap: 6px; align-items: flex-start;
      font-size: 12px; line-height: 1.4; color: var(--car-700);
      background: var(--car-50); border-radius: 12px;
      padding: 12px; margin-top: 16px;
    }
  </style>
</head>
<body>
  <div class="card">
    <div class="logo">Kết nối Wi-Fi</div>
    <div class="subtitle">Chọn mạng để chiếc hộp lên mạng và nhận tin nhắn.</div>
    <form action="/save" method="POST">
      <div class="input-group">
        <label>Mạng xung quanh
          <button type="button" class="rescan" id="rescan">Quét lại</button>
        </label>
        <div class="netlist" id="netlist"><div class="msg">Đang quét…</div></div>
      </div>
      <div class="input-group">
        <label for="ssid">Tên Wi-Fi (SSID)</label>
        <input type="text" id="ssid" name="ssid" placeholder="Chọn ở trên hoặc tự nhập" required autocomplete="off">
      </div>
      <div class="input-group">
        <label for="password">Mật khẩu Wi-Fi</label>
        <div style="position: relative; display: flex; align-items: center;">
          <input type="password" id="password" name="password" placeholder="Để trống nếu mạng không có mật khẩu" style="padding-right: 44px;">
          <button type="button" class="eye" onclick="togglePass()" aria-label="Hiện mật khẩu">&#128065;</button>
        </div>
      </div>
      <input type="submit" value="Lưu và kết nối">
    </form>
    <!-- ESP32-C3 chi co radio 2.4 GHz: mang 5 GHz khong bao gio hien trong danh
         sach tren. Day la ly do hong hay gap nhat, phai noi truoc. -->
    <div class="tips">Hộp chỉ thấy được mạng 2.4 GHz. Mạng 5 GHz sẽ không hiện trong danh sách.</div>
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
