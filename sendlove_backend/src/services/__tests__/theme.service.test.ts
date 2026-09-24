import { ThemeService, sanitizeWidgets, BG_BYTES } from '../theme.service';
import { AppError } from '../../middleware/error-handler.middleware';
import { crc32 } from '../../utils/crc32';

const clock = { type: 'clock_time', x: 50, y: 30, w: 160, h: 45, color: '#000000', align: 'center', font: 'Font7' };

describe('sanitizeWidgets', () => {
  it('giữ đúng khoá firmware đọc theo từng type, bỏ khoá thừa', () => {
    const out = sanitizeWidgets([
      { ...clock, id: 'x', label: 'Giờ', sample: '21:47' },
      { type: 'battery_icon', x: 154, y: 10, w: 75, h: 16, color: '#FF0000', font: 'Font7' },
      { type: 'wifi_icon', x: 0, y: 0, w: 24, h: 24, color: '#abcdef', align: 'center' },
    ]);
    expect(out[0]).toEqual({ type: 'clock_time', x: 50, y: 30, w: 160, h: 45, color: '#000000', align: 'center', font: 'Font7' });
    expect(out[1]).toEqual({ type: 'battery_icon', x: 154, y: 10, w: 75, h: 16 });
    expect(out[2]).toEqual({ type: 'wifi_icon', x: 0, y: 0, w: 24, h: 24, color: '#ABCDEF' });
  });

  it('điền mặc định giống firmware: màu đen, căn trái, phông có sẵn, ngày tiếng Việt', () => {
    const [w] = sanitizeWidgets([{ type: 'clock_date', x: 9, y: 9, w: 140, h: 16 }]);
    expect(w).toEqual({
      type: 'clock_date', x: 9, y: 9, w: 140, h: 16, color: '#000000', align: 'left',
      font: 'Font2', format: 'WD, DD.MM', locale: 'vi',
    });
  });

  it('giữ family + px của phông VLW để web mở lại trình sửa, bỏ giá trị lạ', () => {
    const [a, b] = sanitizeWidgets([
      { ...clock, font: 'f_time', family: 'Chakra Petch', px: 44 },
      { ...clock, font: 'f_time', family: '', px: 999 },
    ]);
    expect(a).toMatchObject({ font: 'f_time', family: 'Chakra Petch', px: 44 });
    expect(b).not.toHaveProperty('family');
    expect(b).not.toHaveProperty('px');
  });

  it.each([
    [[{ ...clock, type: undefined }], 'type'],
    [[{ ...clock, type: 'image' }], 'type'],
    [[{ ...clock, x: 100, w: 160 }], 'outside'],
    [[{ ...clock, x: 1.5 }], 'integer'],
    [[{ ...clock, color: '#000' }], 'color'],
    [[{ ...clock, font: 'f_date' }], 'font'], // phông của clock_date, không phải clock_time
    [[{ ...clock, font: 'Orbitron_32' }], 'font'], // phông biên dịch cứng đã gỡ khỏi firmware
    [[{ ...clock, align: 'justify' }], 'align'],
    [[{ type: 'clock_date', x: 0, y: 0, w: 100, h: 16, format: 'MM-DD' }], 'format'],
    [[{ type: 'clock_date', x: 0, y: 0, w: 100, h: 16, locale: 'fr' }], 'locale'],
    [[], 'non-empty'],
    [Array(9).fill(clock), 'At most'],
  ])('từ chối widget sai %#', (widgets, msg) => {
    expect(() => sanitizeWidgets(widgets)).toThrow(new RegExp(msg as string));
  });
});

const make = (prevRev?: number) => {
  const boxRepo: any = {
    getById: jest.fn().mockResolvedValue(prevRev ? { config: { theme: { rev: prevRev } } } : { config: {} }),
    update: jest.fn(), updateFlags: jest.fn(),
  };
  const storageRepo: any = {
    fileExists: jest.fn().mockResolvedValue(true), getFileMetadata: jest.fn(), generateUploadPolicy: jest.fn(),
    downloadToBuffer: jest.fn().mockResolvedValue(Buffer.alloc(BG_BYTES, 7)),
    generateDownloadUrl: jest.fn(async (p: string) => `https://signed/${p}`),
  };
  return { boxRepo, storageRepo, svc: new ThemeService(boxRepo, storageRepo) };
};

describe('ThemeService.getTheme', () => {
  it('null khi chưa lưu, kèm URL ký khi có nền', async () => {
    const { boxRepo, svc } = make();
    boxRepo.getById.mockResolvedValueOnce({ config: {} });
    expect(await svc.getTheme('b')).toBeNull();
    boxRepo.getById.mockResolvedValueOnce({ config: { theme: { theme_name: 'A', widgets: [], background: 'media/b/theme/bg_1.bin' } } });
    expect((await svc.getTheme('b'))?.background_url).toBe('https://signed/media/b/theme/bg_1.bin');
  });
});

describe('ThemeService.saveTheme', () => {

  it('ghi config/theme với theme_id + rev, bật theme_flag', async () => {
    const { boxRepo, svc } = make();
    const res = await svc.saveTheme('box_1', 'u1', { theme_name: ' Của em ', widgets: [clock] });
    expect(res.theme_name).toBe('Của em');
    expect(res.background).toBeNull();
    expect(res.rev).toBe(1);
    expect(res.theme_id).toMatch(/^t_\d+$/);
    expect(boxRepo.update).toHaveBeenCalledWith('box_1', expect.objectContaining({ 'config/theme': expect.objectContaining({ updated_by: 'u1' }) }));
    expect(boxRepo.updateFlags).toHaveBeenCalledWith('box_1', { theme_flag: true });
  });

  it('rev tăng từ bản đang lưu: hộp so rev để biết phải tải lại', async () => {
    const { svc } = make(6);
    expect((await svc.saveTheme('box_1', 'u1', { theme_name: 'A', widgets: [clock] })).rev).toBe(7);
  });

  it('nhận ảnh nền đúng thư mục + đúng 115200 byte, tự đo size + crc32', async () => {
    const { storageRepo, svc } = make();
    const bg = Buffer.alloc(BG_BYTES, 3);
    storageRepo.downloadToBuffer.mockResolvedValue(bg);
    const res = await svc.saveTheme('box_1', 'u1', { theme_name: 'A', widgets: [clock], background: 'media/box_1/theme/bg_1.bin' });
    expect(res.background).toBe('media/box_1/theme/bg_1.bin');
    expect(res.assets?.bg).toEqual({ path: 'media/box_1/theme/bg_1.bin', size: BG_BYTES, crc32: crc32(bg) });
  });

  it.each([
    'media/box_2/theme/bg_1.bin',        // hộp khác
    'media/box_1/msg_1/video.bin',       // file tin nhắn
    'media/box_1/theme/../msg/x.bin',    // thoát thư mục
  ])('từ chối ảnh nền ngoài thư mục theme: %s', async (path) => {
    const { svc } = make();
    await expect(svc.saveTheme('box_1', 'u1', { theme_name: 'A', widgets: [clock], background: path })).rejects.toBeInstanceOf(AppError);
  });

  it('từ chối ảnh nền sai kích thước', async () => {
    const { storageRepo, boxRepo, svc } = make();
    storageRepo.downloadToBuffer.mockResolvedValue(Buffer.alloc(1000));
    await expect(svc.saveTheme('box_1', 'u1', { theme_name: 'A', widgets: [clock], background: 'media/box_1/theme/bg_1.bin' }))
      .rejects.toThrow(/115200/);
    expect(boxRepo.update).not.toHaveBeenCalled();
  });

  it('widget dùng phông f_time thì bắt buộc kèm file phông, và đưa nó vào assets', async () => {
    const { storageRepo, svc } = make();
    const w = [{ ...clock, font: 'f_time' }];
    await expect(svc.saveTheme('box_1', 'u1', { theme_name: 'A', widgets: w })).rejects.toThrow(/fonts.f_time/);

    const vlw = Buffer.alloc(3000, 1);
    storageRepo.downloadToBuffer.mockResolvedValue(vlw);
    const res = await svc.saveTheme('box_1', 'u1', {
      theme_name: 'A', widgets: w, fonts: { f_time: 'media/box_1/theme/f_1.vlw' },
    });
    expect(res.assets?.f_time).toEqual({ path: 'media/box_1/theme/f_1.vlw', size: 3000, crc32: crc32(vlw) });
  });
});
