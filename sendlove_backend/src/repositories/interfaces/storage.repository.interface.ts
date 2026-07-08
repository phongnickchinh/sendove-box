export interface IStorageRepository {
  generateUploadUrl(filePath: string, contentType: string, expiresInMinutes?: number): Promise<string>;
  generateDownloadUrl(filePath: string, expiresInMinutes?: number): Promise<string>;
  deleteFile(filePath: string): Promise<void>;
  deleteDirectory(directoryPath: string): Promise<void>;
  downloadToLocal(filePath: string, localDestination: string): Promise<void>;
  uploadFromLocal(localFilePath: string, destinationPath: string, contentType?: string): Promise<void>;
}
