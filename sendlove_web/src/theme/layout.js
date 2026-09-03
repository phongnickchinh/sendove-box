/**
 * Hợp đồng theme cho màn hình hộp — ĐỀ XUẤT, chưa có thật.
 *
 * Toàn bộ dấu vết theme trong firmware hôm nay là đúng một khoá "theme_name" ở
 * src/main.cpp:55, mà LayoutEngine::loadConfig() KHÔNG đọc. Không có route nhận
 * theme, không có khoá NVS, IStorageProvider không có API ghi file tuỳ ý
 * (SDStorageProvider.cpp:5-14 mới chỉ sinh /media/*.bin).
 *
 * Vì vậy màn 19-21 chạy trên dữ liệu ở file này, không gọi API nào. Mỗi giá trị
 * dưới đây tra được ngược về một dòng trong LayoutEngine.cpp — không bịa.
 */

/** config.h:17-18 SCREEN_WIDTH/HEIGHT. StandbyBackground.h:8-9 BG_WIDTH/HEIGHT. */
export const SCREEN = 240;

/** Nền là mảng RGB565: 240 × 240 × 2 byte. */
export const BG_BYTES = SCREEN * SCREEN * 2; // 115.200

/** Phiên bản firmware đầu tiên đọc được hai file này. */
export const NEEDS_FW = '2.2';

/**
 * drawClockTime/drawClockDate là if-chain ba nhánh cứng, không phải ô nhập tự
 * do. Tên khác rơi hết về nhánh mặc định.
 */
export const FONTS = [
  { value: 'ChakraPetch_SemiBold_48', label: 'Chakra Petch 48' },
  { value: 'Orbitron_32', label: 'Orbitron 32' },
  { value: 'Font7', label: 'Font 7' },
];

/** drawTextWidget: 'center' / 'right' / còn lại đều là trái. */
export const ALIGNS = [
  { value: 'left', icon: 'alignL', label: 'Căn trái' },
  { value: 'center', icon: 'alignC', label: 'Căn giữa' },
  { value: 'right', icon: 'alignR', label: 'Căn phải' },
];

/**
 * Sàn tối thiểu của w×h. drawTextWidget gọi drawBackgroundPatch xoá đúng ô w×h
 * TRƯỚC khi vẽ chữ mới — ô hẹp hơn chữ thì phần thừa của lần vẽ trước không bị
 * xoá và đọng lại trên màn.
 */
export const MIN_SIZE = {
  clock_time: { w: 132, h: 35 },
  clock_date: { w: 140, h: 16 },
  battery_icon: { w: 75, h: 16 },
};

/** Đúng defaultLayoutJson biên dịch cứng trong main.cpp:54-61. */
export const DEFAULT_WIDGETS = [
  { id: 'clock_date', type: 'clock_date', label: 'Ngày', icon: 'info',
    x: 9, y: 9, w: 140, h: 16, color: '#B83D3D', align: 'left', font: 'Font7', sample: 'THỨ BẢY, 29.08' },
  { id: 'clock_time', type: 'clock_time', label: 'Giờ', icon: 'clock',
    x: 50, y: 30, w: 160, h: 45, color: '#000000', align: 'center', font: 'ChakraPetch_SemiBold_48', sample: '21:47' },
  /* drawBatteryIcon dùng pushImage một ảnh nhiều màu và BỎ QUA cfg.color; mức
     pin là `int state = 3;` viết cứng, không đọc pin thật. Nên widget này không
     có ô chọn màu, và không hứa hẹn phần trăm nào. */
  { id: 'battery_icon', type: 'battery_icon', label: 'Pin', icon: 'battery',
    x: 154, y: 10, w: 75, h: 16, fixed: true },
];

export const THEMES = [
  { id: 'default', name: 'Mặc định', file: null, size: null,
    what: 'Ngày, giờ lớn, biểu tượng pin', bars: [[16, 3], [22, 9]], builtin: true },
  { id: 'big_clock', name: 'Đồng hồ lớn', file: '/theme/big_clock.json', size: '1,1 KB',
    what: 'Chỉ có giờ, Orbitron 32', bars: [[28, 12]] },
  { id: 'quiet', name: 'Tối giản', file: '/theme/quiet.json', size: '0,9 KB',
    what: 'Ngày và giờ, không gì khác', bars: [[14, 3], [20, 7]] },
];

/** Hai file sẽ được ghi lên thẻ SD. */
export const FILES = [
  { path: '/theme/current.json', size: '1,2 KB',
    what: 'Danh sách widget kèm vị trí, kích thước, màu, căn lề và tên phông.' },
  { path: '/theme/bg.bin', size: '115,2 KB',
    what: `Nền ${SCREEN} × ${SCREEN} RGB565 thô — hai byte mỗi điểm ảnh, hộp không cần bộ giải mã.` },
];
