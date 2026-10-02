import { storage } from '../../firebase';
import { IStorageRepository } from '../interfaces/storage.repository.interface';

export class FirebaseStorageRepository implements IStorageRepository {
  /** Signed POST policy for a direct upload to Storage, with a size limit. */
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

  /** Signed download URL. */
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

  async deleteDirectory(directoryPath: string): Promise<void> {
    const bucket = storage.bucket();
    await bucket.deleteFiles({ prefix: directoryPath });
  }

  async downloadToLocal(filePath: string, localDestination: string): Promise<void> {
    const bucket = storage.bucket();
    const file = bucket.file(filePath);
    await file.download({ destination: localDestination });
  }

  async downloadToBuffer(filePath: string): Promise<Buffer> {
    const [buf] = await storage.bucket().file(filePath).download();
    return buf;
  }

  async uploadFromLocal(localFilePath: string, destinationPath: string, contentType?: string): Promise<void> {
    const bucket = storage.bucket();
    await bucket.upload(localFilePath, {
      destination: destinationPath,
      metadata: contentType ? { contentType } : undefined,
    });
  }

  async getFileMetadata(filePath: string): Promise<any> {
    const bucket = storage.bucket();
    const file = bucket.file(filePath);
    const [metadata] = await file.getMetadata();
    return metadata;
  }

  async fileExists(filePath: string): Promise<boolean> {
    const bucket = storage.bucket();
    const file = bucket.file(filePath);
    const [exists] = await file.exists();
    return exists;
  }
}
