"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
const message_service_1 = require("../message.service");
const error_handler_middleware_1 = require("../../middleware/error-handler.middleware");
const makeRepos = () => {
    const msgRepo = {
        createMessage: jest.fn(),
        getMessage: jest.fn(),
        listMessages: jest.fn(),
        countMessagesSince: jest.fn(),
        softDelete: jest.fn(),
    };
    const storageRepo = {
        generateUploadPolicy: jest.fn(),
        generateDownloadUrl: jest.fn(async (path) => `https://signed/${path}`),
        deleteFile: jest.fn(),
        deleteDirectory: jest.fn(),
        downloadToLocal: jest.fn(),
        uploadFromLocal: jest.fn(),
        getFileMetadata: jest.fn(),
        fileExists: jest.fn(),
    };
    return { msgRepo, storageRepo, service: new message_service_1.MessageService(msgRepo, storageRepo) };
};
describe('MessageService.getMessageDetails', () => {
    it('ký URL cho file trình duyệt phát được, không ký bin', async () => {
        const { msgRepo, storageRepo, service } = makeRepos();
        msgRepo.getMessage.mockResolvedValue({
            id: 'msg_1', type: 'video', timestamp: 1,
            bin_url: 'media/b/msg_1/video.bin',
            video_url: 'media/b/msg_1/original.mp4',
            thumbnail_url: 'media/b/msg_1/thumb.jpg',
            voice_url: 'media/b/msg_1/voice.wav',
        });
        const res = await service.getMessageDetails('b', 'msg_1');
        expect(res.media).toEqual({
            video: 'https://signed/media/b/msg_1/original.mp4',
            thumbnail: 'https://signed/media/b/msg_1/thumb.jpg',
            voice: 'https://signed/media/b/msg_1/voice.wav',
        });
        expect(storageRepo.generateDownloadUrl).not.toHaveBeenCalledWith('media/b/msg_1/video.bin', expect.anything());
        expect(storageRepo.generateDownloadUrl).toHaveBeenCalledWith('media/b/msg_1/original.mp4', 15);
        // Path thô vẫn giữ nguyên, không bị ghi đè bằng URL.
        expect(res.video_url).toBe('media/b/msg_1/original.mp4');
    });
    it('dùng gif khi không có ảnh gốc', async () => {
        const { msgRepo, service } = makeRepos();
        msgRepo.getMessage.mockResolvedValue({ id: 'm', type: 'gif', timestamp: 1, gif_url: 'g.gif' });
        const res = await service.getMessageDetails('b', 'm');
        expect(res.media).toEqual({ image: 'https://signed/g.gif' });
    });
    it('một file ký lỗi không làm hỏng cả kết quả', async () => {
        const { msgRepo, storageRepo, service } = makeRepos();
        msgRepo.getMessage.mockResolvedValue({ id: 'm', type: 'image', timestamp: 1, image_url: 'a.jpg', bg_music_url: 'm.wav' });
        storageRepo.generateDownloadUrl.mockImplementation(async (p) => {
            if (p === 'a.jpg')
                throw new Error('boom');
            return `https://signed/${p}`;
        });
        jest.spyOn(console, 'error').mockImplementation(() => { });
        const res = await service.getMessageDetails('b', 'm');
        expect(res.media).toEqual({ bg_music: 'https://signed/m.wav' });
    });
    it('404 khi không có tin', async () => {
        const { msgRepo, service } = makeRepos();
        msgRepo.getMessage.mockResolvedValue(null);
        await expect(service.getMessageDetails('b', 'x')).rejects.toBeInstanceOf(error_handler_middleware_1.AppError);
    });
});
describe('MessageService.getMessages', () => {
    it('ký thumbnail cho từng tin có ảnh thu nhỏ, bỏ qua tin không có', async () => {
        const { msgRepo, storageRepo, service } = makeRepos();
        msgRepo.listMessages.mockResolvedValue([
            { id: 'a', type: 'video', timestamp: 2, thumbnail_url: 't/a.jpg' },
            { id: 'b', type: 'text', timestamp: 1, text: 'hi' },
        ]);
        const res = await service.getMessages('box', 20);
        expect(res[0].thumbnail).toBe('https://signed/t/a.jpg');
        expect(res[1]).not.toHaveProperty('thumbnail');
        expect(storageRepo.generateDownloadUrl).toHaveBeenCalledTimes(1);
        expect(msgRepo.listMessages).toHaveBeenCalledWith('box', 20);
    });
    it('một thumbnail ký lỗi không làm hỏng danh sách', async () => {
        const { msgRepo, storageRepo, service } = makeRepos();
        msgRepo.listMessages.mockResolvedValue([{ id: 'a', type: 'image', timestamp: 1, thumbnail_url: 'x.jpg' }]);
        storageRepo.generateDownloadUrl.mockRejectedValue(new Error('boom'));
        jest.spyOn(console, 'error').mockImplementation(() => { });
        const res = await service.getMessages('box');
        expect(res).toEqual([{ id: 'a', type: 'image', timestamp: 1, thumbnail_url: 'x.jpg' }]);
    });
});
//# sourceMappingURL=message.service.test.js.map