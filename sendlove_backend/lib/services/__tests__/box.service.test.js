"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
const box_service_1 = require("../box.service");
const mock_box_repository_1 = require("../../repositories/mocks/mock-box.repository");
const mock_user_repository_1 = require("../../repositories/mocks/mock-user.repository");
const error_handler_middleware_1 = require("../../middleware/error-handler.middleware");
describe('BoxService', () => {
    let boxService;
    let mockBoxRepo;
    let mockUserRepo;
    beforeEach(() => {
        mockBoxRepo = new mock_box_repository_1.MockBoxRepository();
        mockUserRepo = new mock_user_repository_1.MockUserRepository();
        boxService = new box_service_1.BoxService(mockBoxRepo, mockUserRepo);
    });
    describe('pairBox', () => {
        it('should successfully pair a user as a sender', async () => {
            // Setup
            mockBoxRepo.findByPairingCode.mockResolvedValue({ id: 'box123', pairing: {} });
            mockBoxRepo.update.mockResolvedValue(undefined);
            mockBoxRepo.updateFlags.mockResolvedValue(undefined);
            mockUserRepo.linkBox.mockResolvedValue(undefined);
            // Execute
            const result = await boxService.pairBox('user_123', 'S12345678', 'My Box');
            // Verify
            expect(result).toEqual({ boxId: 'box123', role: 'sender' });
            expect(mockBoxRepo.findByPairingCode).toHaveBeenCalledWith('S12345678', 'scode');
            expect(mockBoxRepo.update).toHaveBeenCalled();
            expect(mockBoxRepo.updateFlags).toHaveBeenCalledWith('box123', { p_flag: true });
            expect(mockUserRepo.linkBox).toHaveBeenCalledWith('user_123', 'box123', { role: 'sender', box_name: 'My Box' });
        });
        it('should throw 404 if pairing code is invalid', async () => {
            mockBoxRepo.findByPairingCode.mockResolvedValue(null);
            await expect(boxService.pairBox('user_123', 'R12345678', 'Box')).rejects.toThrow(error_handler_middleware_1.AppError);
            await expect(boxService.pairBox('user_123', 'R12345678', 'Box')).rejects.toMatchObject({
                statusCode: 404,
                code: 'box_not_found'
            });
        });
        it('should throw 400 if sender slot is already taken', async () => {
            mockBoxRepo.findByPairingCode.mockResolvedValue({
                id: 'box123',
                pairing: { sender_id: 'another_user' }
            });
            await expect(boxService.pairBox('user_123', 'S12345678', 'Box')).rejects.toThrow(error_handler_middleware_1.AppError);
        });
        it('should throw 400 if user tries to take both slots', async () => {
            mockBoxRepo.findByPairingCode.mockResolvedValue({
                id: 'box123',
                pairing: { receiver_id: 'user_123' } // Current user is already receiver
            });
            // User tries to pair as sender
            await expect(boxService.pairBox('user_123', 'S12345678', 'Box')).rejects.toMatchObject({
                statusCode: 400,
                code: 'conflict_role'
            });
        });
    });
    describe('updateWifi', () => {
        beforeEach(() => {
            mockBoxRepo.getById.mockResolvedValue({ id: 'box123', pairing: { receiver_id: 'user_123' } });
            mockBoxRepo.update.mockResolvedValue(undefined);
        });
        it('keeps the stored password when none is sent', async () => {
            await boxService.updateWifi('user_123', 'box123', 'Nha_Duyen');
            const updates = mockBoxRepo.update.mock.calls[0][1];
            expect(updates['config/wifi_config/ssid']).toBe('Nha_Duyen');
            expect(updates).not.toHaveProperty('config/wifi_config');
        });
        it('writes ssid and password together when a password is sent', async () => {
            await boxService.updateWifi('user_123', 'box123', 'Nha_Duyen', 'secret123');
            expect(mockBoxRepo.update.mock.calls[0][1]['config/wifi_config']).toEqual({ ssid: 'Nha_Duyen', pwd: 'secret123' });
        });
        it('stores an empty password only when the user chose an open network', async () => {
            await boxService.updateWifi('user_123', 'box123', 'Cafe', '');
            expect(mockBoxRepo.update.mock.calls[0][1]['config/wifi_config']).toEqual({ ssid: 'Cafe', pwd: '' });
        });
    });
});
//# sourceMappingURL=box.service.test.js.map