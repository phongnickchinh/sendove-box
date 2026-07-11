"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.FirebaseStorageRepository = void 0;
const firebase_1 = require("../../firebase");
class FirebaseStorageRepository {
    /**
     * Generates a signed POST policy for uploading a file directly to Firebase Storage with size limits.
     */
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
    /**
     * Retrieves metadata of a file.
     */
    async getFileMetadata(filePath) {
        const bucket = firebase_1.storage.bucket();
        const file = bucket.file(filePath);
        const [metadata] = await file.getMetadata();
        return metadata;
    }
    /**
     * Checks if a file exists.
     */
    async fileExists(filePath) {
        const bucket = firebase_1.storage.bucket();
        const file = bucket.file(filePath);
        const [exists] = await file.exists();
        return exists;
    }
}
exports.FirebaseStorageRepository = FirebaseStorageRepository;
//# sourceMappingURL=firebase-storage.repository.js.map