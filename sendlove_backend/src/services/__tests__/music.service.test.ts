import { MusicService, crc32 } from '../music.service';

/** File đúng định dạng hộp: AUDC + u16 16000 + u32 size + RIFF...WAVE (54 byte header) + PCM */
const audc = (rate = 16000, pcm = 64) => {
  const b = Buffer.alloc(54 + pcm);
  b.write('AUDC', 0, 'ascii');
  b.writeUInt16LE(rate, 4);
  b.writeUInt32LE(44 + pcm, 6);
  b.write('RIFF', 10, 'ascii');
  b.write('WAVE', 18, 'ascii');
  return b;
};

const make = (existing: any = null, count = 0) => {
  const musicRepo: any = {
    list: jest.fn().mockResolvedValue(Array(count).fill({})),
    get: jest.fn().mockResolvedValue(existing),
    save: jest.fn(),
    rename: jest.fn(),
    removeAndDetach: jest.fn().mockResolvedValue(2),
  };
  const storageRepo: any = {
    generateUploadPolicy: jest.fn().mockResolvedValue({ url: 'u', fields: {} }),
    fileExists: jest.fn().mockResolvedValue(true),
    downloadToBuffer: jest.fn().mockResolvedValue(audc()),
    deleteFile: jest.fn().mockResolvedValue(undefined),
    generateDownloadUrl: jest.fn().mockResolvedValue('signed'),
  };
  return { svc: new MusicService(musicRepo, storageRepo), musicRepo, storageRepo };
};

const body = { music_id: 'mus_1790000000000', rev: 1, name: 'Chuông sáng', duration_ms: 30000 };

describe('crc32', () => {
  it('khớp CRC-32 chuẩn zlib (giá trị kiểm "123456789" = CBF43926), cùng công thức firmware', () => {
    expect(crc32(Buffer.from('123456789'))).toBe(0xcbf43926);
  });
});

describe('MusicService', () => {
  it('bài mới: cấp policy ở media/{box}/music/{id}_r1.aud, chưa ghi DB', async () => {
    const { svc, musicRepo, storageRepo } = make();
    const r = await svc.initiateUpload('b1');
    expect(r.rev).toBe(1);
    expect(r.path).toBe(`media/b1/music/${r.music_id}_r1.aud`);
    expect(storageRepo.generateUploadPolicy).toHaveBeenCalledWith(r.path, 'application/octet-stream', 2_000_000, 15);
    expect(musicRepo.save).not.toHaveBeenCalled();
  });

  it('chặn bài thứ 11', async () => {
    const { svc } = make(null, 10);
    await expect(svc.initiateUpload('b1')).rejects.toThrow(/Maximum 10/);
  });

  it('commit tự đo size + crc từ file thật, không tin web', async () => {
    const { svc, musicRepo, storageRepo } = make();
    const file = audc();
    storageRepo.downloadToBuffer.mockResolvedValue(file);
    const m = await svc.commit('b1', 'uid', { ...body, size: 1, crc32: 1 });
    expect(m.size).toBe(file.length);
    expect(m.crc32).toBe(crc32(file));
    expect(m.storage_path).toBe('media/b1/music/mus_1790000000000_r1.aud');
    expect(musicRepo.save).toHaveBeenCalledWith('b1', m);
  });

  it('commit từ chối file sai tần số và xoá file vừa tải lên', async () => {
    const { svc, musicRepo, storageRepo } = make();
    storageRepo.downloadToBuffer.mockResolvedValue(audc(8000));
    await expect(svc.commit('b1', 'uid', body)).rejects.toThrow(/16000/);
    expect(storageRepo.deleteFile).toHaveBeenCalled();
    expect(musicRepo.save).not.toHaveBeenCalled();
  });

  it.each([
    [{ ...body, duration_ms: 3000 }, /5-60/],
    [{ ...body, name: '' }, /name/],
    [{ ...body, music_id: '../x' }, /music_id/],
    [{ ...body, rev: 2 }, /changed/],  // bài mới phải là rev 1
  ])('commit từ chối đầu vào sai %#', async (b, msg) => {
    const { svc } = make();
    await expect(svc.commit('b1', 'uid', b)).rejects.toThrow(msg);
  });

  it('thay nội dung: rev + 1, giữ created_at, xoá file bản cũ', async () => {
    const old = { ...body, rev: 1, created_at: 5, created_by: 'a', storage_path: 'media/b1/music/mus_1790000000000_r1.aud' };
    const { svc, storageRepo } = make(old);
    const m = await svc.commit('b1', 'uid', { ...body, rev: 2 });
    expect(m.rev).toBe(2);
    expect(m.created_at).toBe(5);
    expect(storageRepo.deleteFile).toHaveBeenCalledWith(old.storage_path);
  });

  it('xoá bài: gỡ khỏi báo thức trong cùng lần ghi và báo số báo thức bị gỡ', async () => {
    const { svc, musicRepo, storageRepo } = make({ ...body, storage_path: 'p' });
    await expect(svc.remove('b1', body.music_id)).resolves.toEqual({ detached_alarms: 2 });
    expect(musicRepo.removeAndDetach).toHaveBeenCalledWith('b1', body.music_id);
    expect(storageRepo.deleteFile).toHaveBeenCalledWith('p');
  });
});
