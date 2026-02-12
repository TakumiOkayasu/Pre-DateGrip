export interface Manageable {
  addQuery: (connectionId?: string | null) => void;
  removeQuery: (id: string) => void;
  updateQuery: (id: string, content: string) => void;
  renameQuery: (id: string, name: string) => void;
  setActive: (id: string | null) => void;
}
