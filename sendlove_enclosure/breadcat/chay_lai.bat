@echo off
rem Dung lai vo SendLove Breadcat tu breadcat.py (sau khi sua thong so).
rem Bam dup file nay. Mat khoang 3 phut. Ket qua: vo_than.stl, tam_day.stl, tai_phai.stl, tai_trai.stl,
rem breadcat_vo.blend, anh shape_*.png va ky_thuat_*.png, bang kiem tra kiem_tra.txt
cd /d "%~dp0"
set BL="C:\Program Files\Blender Foundation\Blender 5.2\blender.exe"
if not exist %BL% (
  echo Khong tim thay Blender tai %BL%
  echo Sua dong "set BL=" trong file nay cho dung duong dan blender.exe
  pause
  exit /b 1
)
echo [1/2] Dung vo, khoet lo, kiem tra va cham, xuat STL ... (khoang 2 phut)
%BL% -b --factory-startup --python-exit-code 1 --python breadcat.py -- mech "%~dp0." > chay_lai.log 2>&1
if errorlevel 1 goto loi
echo [2/2] Ve anh xem truoc ... (khoang 1 phut)
set BC_SAMPLES=16
set BC_W=800
%BL% -b --factory-startup --python-exit-code 1 --python breadcat.py -- shape "%~dp0." >> chay_lai.log 2>&1
if errorlevel 1 goto loi
rem Blender luu ban .blend cu thanh .blend1 -> giu lai 1 doi (phong khi da luu de sua tay vao breadcat_vo.blend)
if exist breadcat_vo.blend1 move /y breadcat_vo.blend1 breadcat_vo_ban_truoc.blend >nul
if exist breadcat_shape.blend1 del breadcat_shape.blend1
echo.
echo ===== KET QUA KIEM TRA =====
type kiem_tra.txt
echo.
echo Xong. canh_hong=0, manh=1 va khong co dong nao "cham_...=True" la dat.
pause
exit /b 0
:loi
echo.
echo ===== CO LOI - 30 dong cuoi cua chay_lai.log =====
powershell -NoProfile -Command "Get-Content chay_lai.log -Tail 30"
pause
exit /b 1
