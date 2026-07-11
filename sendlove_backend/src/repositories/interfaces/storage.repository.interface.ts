export interface IStorageRepository {
  generateUploadPolicy(filePath: string, contentType: string, maxSizeInBytes: number, expiresInMinutes?: number): Promise<{ url: string; fields: Record<string, string> }>;
  generateDownloadUrl(filePath: string, expiresInMinutes?: number): Promise<string>;
  deleteFile(filePath: string): Promise<void>;
  deleteDirectory(directoryPath: string): Promise<void>;
  downloadToLocal(filePath: string, localDestination: string): Promise<void>;
  uploadFromLocal(localFilePath: string, destinationPath: string, contentType?: string): Promise<void>;
  getFileMetadata(filePath: string): Promise<any>;
  fileExists(filePath: string): Promise<boolean>;
}
