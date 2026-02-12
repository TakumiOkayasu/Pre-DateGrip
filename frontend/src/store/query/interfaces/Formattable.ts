export interface Formattable {
  formatQuery: (id: string) => Promise<void>;
  clearError: (id?: string) => void;
}
