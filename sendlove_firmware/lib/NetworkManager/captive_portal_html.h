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
//
// The "Bao thuc" (2026-09-18) dung <script> RIENG o cuoi trang, khong dung chung
// bien/ham voi script Wi-Fi o tren. Hop dong:
//   GET  /alarms          -> {now, max, dirty, items:[{id,time,en,rep}]}
//   POST /alarms/save     id (rong = them), time "HH:MM", en 0/1, rep 0/1
//   POST /alarms/delete   id
//   POST /time            epoch (giay) — gui gio dien thoai khi hop chua co NTP
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
    .wrap { width: 100%; max-width: 390px; display: flex; flex-direction: column; gap: 16px; align-self: flex-start; }
    .alist { border: 1px solid var(--car-300); border-radius: 8px; margin-bottom: 16px; }
    .alist .msg { padding: 12px; color: var(--n-500); font-size: 12px; line-height: 1.4; }
    .arow {
      display: flex; align-items: center; gap: 10px;
      padding: 10px 12px; border-bottom: 1px solid var(--n-100);
    }
    .arow:last-child { border-bottom: none; }
    .arow .t { font-size: 22px; font-weight: 700; color: var(--car-900); min-width: 70px; }
    .arow.off .t { color: var(--n-400); }
    .arow .rep { flex: 1; font-size: 12px; color: var(--n-500); }
    .arow input[type=checkbox] { width: 22px; height: 22px; accent-color: var(--rose-400); }
    .del {
      background: none; border: none; color: var(--rose-700); font-size: 20px;
      line-height: 1; padding: 4px 6px; cursor: pointer;
    }
    .addrow { display: flex; gap: 8px; }
    .addrow input[type=time], .addrow select {
      flex: 1; padding: 10px 12px; min-height: 44px;
      background: var(--n-0); border: 1px solid var(--car-300); border-radius: 8px;
      color: var(--car-900); font-family: inherit; font-size: 15px;
    }
    .btn {
      min-height: 44px; padding: 0 16px;
      background: var(--rose-400); color: var(--car-900);
      border: none; border-radius: 8px;
      font-family: inherit; font-size: 14px; font-weight: 600; cursor: pointer;
    }
    .btn:disabled { opacity: .5; cursor: default; }
    .aerr { color: var(--rose-700); font-size: 12px; margin-top: 8px; min-height: 16px; }
  </style>
</head>
<body>
  <div class="wrap">
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
  <div class="card">
    <div class="logo">Báo thức</div>
    <div class="subtitle">Đặt ngay trên hộp, không cần Internet. Chạm hộp để báo lại sau 5 phút, giữ 3 giây để tắt.</div>
    <div class="alist" id="alist"><div class="msg">Đang tải…</div></div>
    <div class="addrow">
      <input type="time" id="atime" value="07:00" aria-label="Giờ báo thức">
      <select id="arep" aria-label="Tần suất">
        <option value="1">Mỗi ngày</option>
        <option value="0">Một lần</option>
      </select>
      <button type="button" class="btn" id="aadd">Thêm</button>
    </div>
    <div class="aerr" id="aerr"></div>
    <!-- Luat dong bo nam o AlarmClock.h: sua o day thi len mang se GHI DE danh
         sach tren web. Phai noi truoc, khong nguoi dung tuong web bi loi. -->
    <div class="tips">Khi hộp lên mạng, danh sách này sẽ thay cho danh sách báo thức trên web.</div>
  </div>
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
  <script>
    // Bao thuc — doc lap voi script Wi-Fi o tren.
    (function () {
      var MAX = 10;
      var list = document.getElementById("alist");
      var err = document.getElementById("aerr");
      var addBtn = document.getElementById("aadd");

      function post(url, data) {
        var body = Object.keys(data).map(function (k) {
          return encodeURIComponent(k) + "=" + encodeURIComponent(data[k]);
        }).join("&");
        return fetch(url, {
          method: "POST",
          headers: { "Content-Type": "application/x-www-form-urlencoded" },
          body: body
        }).then(function (r) {
          return r.json().then(function (d) {
            if (!d.ok) throw new Error(d.err || "Lỗi");
            return d;
          });
        });
      }

      function fail(e) { err.textContent = (e && e.message) ? e.message : "Không kết nối được hộp."; }

      function row(a) {
        var r = document.createElement("div");
        r.className = "arow" + (a.en ? "" : " off");
        var t = document.createElement("span");
        t.className = "t";
        t.textContent = a.time;
        var rep = document.createElement("span");
        rep.className = "rep";
        rep.textContent = a.rep ? "Mỗi ngày" : (a.en ? "Một lần" : "Một lần — đã tắt");
        var sw = document.createElement("input");
        sw.type = "checkbox";
        sw.checked = !!a.en;
        sw.setAttribute("aria-label", "Bật báo thức " + a.time);
        sw.onchange = function () {
          err.textContent = "";
          post("/alarms/save", { id: a.id, time: a.time, en: sw.checked ? 1 : 0, rep: a.rep ? 1 : 0 })
            .then(load).catch(function (e) { fail(e); load(); });
        };
        var del = document.createElement("button");
        del.type = "button";
        del.className = "del";
        del.textContent = "×";
        del.setAttribute("aria-label", "Xoá báo thức " + a.time);
        del.onclick = function () {
          err.textContent = "";
          post("/alarms/delete", { id: a.id }).then(load).catch(fail);
        };
        r.appendChild(t); r.appendChild(rep); r.appendChild(sw); r.appendChild(del);
        return r;
      }

      function load() {
        return fetch("/alarms").then(function (r) { return r.json(); }).then(function (d) {
          MAX = d.max || MAX;
          list.innerHTML = "";
          var items = d.items || [];
          items.sort(function (x, y) { return x.time < y.time ? -1 : (x.time > y.time ? 1 : 0); });
          if (!items.length) {
            var m = document.createElement("div");
            m.className = "msg";
            m.textContent = "Chưa có báo thức nào.";
            list.appendChild(m);
          }
          items.forEach(function (a) { list.appendChild(row(a)); });
          addBtn.disabled = items.length >= MAX;
          addBtn.textContent = items.length >= MAX ? "Đã đủ " + MAX : "Thêm";
        }).catch(fail);
      }

      addBtn.onclick = function () {
        var t = document.getElementById("atime").value;
        if (!/^\d{2}:\d{2}$/.test(t)) { err.textContent = "Chọn giờ trước đã."; return; }
        err.textContent = "";
        addBtn.disabled = true;
        post("/alarms/save", { time: t, en: 1, rep: document.getElementById("arep").value })
          .then(load).catch(function (e) { fail(e); load(); });
      };

      // Gui gio dien thoai truoc: hop vua cam dien o AP mode chua co NTP thi
      // khong biet bay gio la may gio, bao thuc se khong bao gio keu.
      post("/time", { epoch: Math.floor(Date.now() / 1000) }).catch(function () {}).then(load);
    })();
  </script>
</body>
</html>
)raw";

#endif // CAPTIVE_PORTAL_HTML_H
