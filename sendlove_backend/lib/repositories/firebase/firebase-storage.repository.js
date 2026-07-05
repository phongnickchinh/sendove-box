"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.FirebaseStorageRepository = void 0;
const firebase_1 = require("../../firebase");
class FirebaseStorageRepository {
    /**
     * Generates a signed URL for uploading a file directly to Firebase Storage.
     */
    async generateUploadUrl(filePath, contentType, expiresInMinutes = 60) {
        const bucket = firebase_1.storage.bucket();
        const file = bucket.file(filePath);
        const [url] = await file.getSignedUrl({
            version: 'v4',
            action: 'write',
            expires: Date.now() + expiresInMinutes * 60 * 1000,
            contentType,
        });
        return url;
    }
    /**
     * Generates a signed URL for downloading a file.
     */
    async generateDownloadUrl(filePath, expiresInMinutes = 60 * 24) {
        const bucket = firebase_1.storage.bucket();
        const file = bucket.file(filePath);
        const [url] = await file.getSignedUrl({
            version: 'v4',
            action: 'read',
            expires: Date.now() + expiresInMinutes * 60 * 1000,
        });
        return url;
    }
    /**
     * Deletes a file from Firebase Storage.
     */
    async deleteFile(filePath) {
        const bucket = firebase_1.storage.bucket();
        const file = bucket.file(filePath);
        try {
            await file.delete();
        }
        catch (error) {
            if (error.code !== 404) {
                throw error; // Rethrow if it's not a "Not Found" error
            }
        }
    }
    /**
     * Delete an entire directory recursively.
     */
    async deleteDirectory(directoryPath) {
        const bucket = firebase_1.storage.bucket();
        await bucket.deleteFiles({ prefix: directoryPath });
    }
    /**
     * Downloads a file from Storage to a local temporary path.
     */
    async downloadToLocal(filePath, localDestination) {
        const bucket = firebase_1.storage.bucket();
        const file = bucket.file(filePath);
        await file.download({ destination: localDestination });
    }
    /**
     * Uploads a local file to Storage.
     */
    async uploadFromLocal(localFilePath, destinationPath, contentType) {
        const bucket = firebase_1.storage.bucket();
        await bucket.upload(localFilePath, {
            destination: destinationPath,
            metadata: contentType ? { contentType } : undefined,
        });
    }
}
exports.FirebaseStorageRepository = FirebaseStorageRepository;
//# sourceMappingURL=firebase-storage.repository.js.map