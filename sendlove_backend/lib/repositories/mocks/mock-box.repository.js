"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.MockBoxRepository = void 0;
class MockBoxRepository {
    constructor() {
        this.getById = jest.fn();
        this.getAll = jest.fn();
        this.create = jest.fn();
        this.update = jest.fn();
        this.delete = jest.fn();
        this.findByPairingCode = jest.fn();
        this.updateFlags = jest.fn();
        this.getFlags = jest.fn();
        this.updateStatus = jest.fn();
        this.getStatus = jest.fn();
    }
}
exports.MockBoxRepository = MockBoxRepository;
//# sourceMappingURL=mock-box.repository.js.map