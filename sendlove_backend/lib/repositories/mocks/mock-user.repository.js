"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.MockUserRepository = void 0;
class MockUserRepository {
    constructor() {
        this.getById = jest.fn();
        this.getAll = jest.fn();
        this.create = jest.fn();
        this.update = jest.fn();
        this.delete = jest.fn();
        this.linkBox = jest.fn();
        this.unlinkBox = jest.fn();
        this.updateLastLogin = jest.fn();
        this.softDelete = jest.fn();
    }
}
exports.MockUserRepository = MockUserRepository;
//# sourceMappingURL=mock-user.repository.js.map