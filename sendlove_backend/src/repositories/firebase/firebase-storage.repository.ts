import { storage } from '../../firebase';
import { IStorageRepository } from '../interfaces/storage.repository.interface';

export class FirebaseStorageRepository implements IStorageRepository {
  /**
   * Generates a signed POST policy for uploading a file directly to Firebase Storage with size limits.
   */
  async generateUploadPolicy(filePath: string, contentType: string, maxSizeInBytes: number, expiresInMinutes: number = 60): Promise<{ url: string; fields: Record<string, string> }> {
    const bucket = storage.bucket();
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

  /**
   * Retrieves metadata of a file.
   */
  async getFileMetadata(filePath: string): Promise<any> {
    const bucket = storage.bucket();
    const file = bucket.file(filePath);
    const [metadata] = await file.getMetadata();
    return metadata;
  }

  /**
   * Checks if a file exists.
   */
  async fileExists(filePath: string): Promise<boolean> {
    const bucket = storage.bucket();
    const file = bucket.file(filePath);
    const [exists] = await file.exists();
    return exists;
  }
}
