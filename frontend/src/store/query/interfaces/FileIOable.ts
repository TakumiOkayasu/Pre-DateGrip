export interface FileIOable {
  saveToFile: (id: string) => Promise<void>;
  loadFromFile: (id: string) => Promise<void>;
}
