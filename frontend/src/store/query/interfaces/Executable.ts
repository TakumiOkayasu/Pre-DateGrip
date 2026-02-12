export interface Executable {
  executeQuery: (id: string, connectionId: string) => Promise<void>;
  executeSelectedText: (id: string, connectionId: string, selectedText: string) => Promise<void>;
  cancelQuery: (connectionId: string) => Promise<void>;
}
