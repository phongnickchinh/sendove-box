import { storage } from '../../firebase';

export class FirebaseStorageRepository {
  /**
   * Generates a signed URL for uploading a file directly to Firebase Storage.
   */
  async generateUploadUrl(filePath: string, contentType: string, expiresInMinutes: number = 60): Promise<string> {
    const bucket = storage.bucket();
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
  async generateDownloadUrl(filePath: string, expiresInMinutes: number = 60 * 24): Promise<string> {
    const bucket = storage.bucket();
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
  async deleteFile(filePath: string): Promise<void> {
    const bucket = storage.bucket();
    const file = bucket.file(filePath);
    try {
      await file.delete();
    } catch (error: any) {
      if (error.code !== 404) {
        throw error; // Rethrow if it's not a "Not Found" error
      }
    }
  }

  /**
   * Delete an entire directory recursively.
   */
  async deleteDirectory(directoryPath: string): Promise<void> {
    const bucket = storage.bucket();
    await bucket.deleteFiles({ prefix: directoryPath });
  }

  /**
   * Downloads a file from Storage to a local temporary path.
   */
  async downloadToLocal(filePath: string, localDestination: string): Promise<void> {
    const bucket = storage.bucket();
    const file = bucket.file(filePath);
    await file.download({ destination: localDestination });
  }

  /**
   * Uploads a local file to Storage.
   */
  async uploadFromLocal(localFilePath: string, destinationPath: string, contentType?: string): Promise<void> {
    const bucket = storage.bucket();
    await bucket.upload(localFilePath, {
      destination: destinationPath,
      metadata: contentType ? { contentType } : undefined,
    });
  }
}
