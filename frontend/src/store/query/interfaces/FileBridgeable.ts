export interface FileBridgeable {
  saveQueryToFile(content: string, defaultFileName?: string): Promise<{ filePath: string }>;
  loadQueryFromFile(): Promise<{ filePath: string; content: string }>;
}
