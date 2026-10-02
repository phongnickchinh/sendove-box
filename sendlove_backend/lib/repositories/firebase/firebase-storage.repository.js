"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.FirebaseStorageRepository = void 0;
const firebase_1 = require("../../firebase");
class FirebaseStorageRepository {
    /** Signed POST policy for a direct upload to Storage, with a size limit. */
    async generateUploadPolicy(filePath, contentType, maxSizeInBytes, expiresInMinutes = 60) {
        const bucket = firebase_1.storage.bucket();
        const file = bucket.file(filePath);
        const [response] = await file.generateSignedPostPolicyV4({
            expires: Date.now() + expiresInMinutes * 60 * 1000,
            conditions: [
                ['content-length-range', 0, maxSizeInBytes],
            ],
            fields: {
                'Content-Type': contentType,
            },
        });
        return {
            url: response.url,
            fields: response.fields,
        };
    }
    /** Signed download URL. */
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
    async deleteDirectory(directoryPath) {
        const bucket = firebase_1.storage.bucket();
        await bucket.deleteFiles({ prefix: directoryPath });
    }
    async downloadToLocal(filePath, localDestination) {
        const bucket = firebase_1.storage.bucket();
        const file = bucket.file(filePath);
        await file.download({ destination: localDestination });
    }
    async downloadToBuffer(filePath) {
        const [buf] = await firebase_1.storage.bucket().file(filePath).download();
        return buf;
    }
    async uploadFromLocal(localFilePath, destinationPath, contentType) {
        const bucket = firebase_1.storage.bucket();
        await bucket.upload(localFilePath, {
            destination: destinationPath,
            metadata: contentType ? { contentType } : undefined,
        });
    }
    async getFileMetadata(filePath) {
        const bucket = firebase_1.storage.bucket();
        const file = bucket.file(filePath);
        const [metadata] = await file.getMetadata();
        return metadata;
    }
    async fileExists(filePath) {
        const bucket = firebase_1.storage.bucket();
        const file = bucket.file(filePath);
        const [exists] = await file.exists();
        return exists;
    }
}
exports.FirebaseStorageRepository = FirebaseStorageRepository;
//# sourceMappingURL=firebase-storage.repository.js.map