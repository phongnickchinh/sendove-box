import { ThemeService, sanitizeWidgets, BG_BYTES } from '../theme.service';
import { AppError } from '../../middleware/error-handler.middleware';

const clock = { type: 'clock_time', x: 50, y: 30, w: 160, h: 45, color: '#000000', align: 'center', font: 'Orbitron_32' };

describe('sanitizeWidgets', () => {
  it('giữ đúng khoá firmware đọc theo từng type, bỏ khoá thừa', () => {
    const out = sanitizeWidgets([
      { ...clock, id: 'x', label: 'Giờ', sample: '21:47' },
      { type: 'battery_icon', x: 154, y: 10, w: 75, h: 16, color: '#FF0000', font: 'Font7' },
      { type: 'wifi_icon', x: 0, y: 0, w: 24, h: 24, color: '#abcdef', align: 'center' },
    ]);
    expect(out[0]).toEqual({ type: 'clock_time', x: 50, y: 30, w: 160, h: 45, color: '#000000', align: 'center', font: 'Orbitron_32' });
    expect(out[1]).toEqual({ type: 'battery_icon', x: 154, y: 10, w: 75, h: 16 });
    expect(out[2]).toEqual({ type: 'wifi_icon', x: 0, y: 0, w: 24, h: 24, color: '#ABCDEF' });
  });

  it('điền mặc định giống firmware: màu đen, căn trái, phông mặc định', () => {
    const [w] = sanitizeWidgets([{ type: 'clock_date', x: 9, y: 9, w: 140, h: 16 }]);
    expect(w).toEqual({ type: 'clock_date', x: 9, y: 9, w: 140, h: 16, color: '#000000', align: 'left', font: 'ChakraPetch_SemiBold_16' });
  });

  it.each([
    [[{ ...clock, type: undefined }], 'type'],
    [[{ ...clock, type: 'image' }], 'type'],
    [[{ ...clock, x: 100, w: 160 }], 'outside'],
    [[{ ...clock, x: 1.5 }], 'integer'],
    [[{ ...clock, color: '#000' }], 'color'],
    [[{ ...clock, font: 'Roboto_14' }], 'font'], // phông của clock_date, không phải clock_time
    [[{ ...clock, align: 'justify' }], 'align'],
    [[], 'non-empty'],
    [Array(9).fill(clock), 'At most'],
  ])('từ chối widget sai %#', (widgets, msg) => {
    expect(() => sanitizeWidgets(widgets)).toThrow(new RegExp(msg as string));
  });
});

describe('ThemeService.saveTheme', () => {
  const make = () => {
    const boxRepo: any = { getById: jest.fn(), update: jest.fn(), updateFlags: jest.fn() };
    const storageRepo: any = { fileExists: jest.fn(), getFileMetadata: jest.fn(), generateUploadPolicy: jest.fn() };
    return { boxRepo, storageRepo, svc: new ThemeService(boxRepo, storageRepo) };
  };

  it('ghi config/theme và bật theme_flag', async () => {
    const { boxRepo, svc } = make();
    const res = await svc.saveTheme('box_1', 'u1', { theme_name: ' Của em ', widgets: [clock] });
    expect(res.theme_name).toBe('Của em');
    expect(res.background).toBeNull();
    expect(boxRepo.update).toHaveBeenCalledWith('box_1', expect.objectContaining({ 'config/theme': expect.objectContaining({ updated_by: 'u1' }) }));
    expect(boxRepo.updateFlags).toHaveBeenCalledWith('box_1', { theme_flag: true });
  });

  it('nhận ảnh nền đúng thư mục của hộp và đúng 115200 byte', async () => {
    const { storageRepo, svc } = make();
    storageRepo.fileExists.mockResolvedValue(true);
    storageRepo.getFileMetadata.mockResolvedValue({ size: String(BG_BYTES) });
    const res = await svc.saveTheme('box_1', 'u1', { theme_name: 'A', widgets: [clock], background: 'media/box_1/theme/bg_1.bin' });
    expect(res.background).toBe('media/box_1/theme/bg_1.bin');
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
    storageRepo.fileExists.mockResolvedValue(true);
    storageRepo.getFileMetadata.mockResolvedValue({ size: '1000' });
    await expect(svc.saveTheme('box_1', 'u1', { theme_name: 'A', widgets: [clock], background: 'media/box_1/theme/bg_1.bin' }))
      .rejects.toThrow(/115200/);
    expect(boxRepo.update).not.toHaveBeenCalled();
  });
});
