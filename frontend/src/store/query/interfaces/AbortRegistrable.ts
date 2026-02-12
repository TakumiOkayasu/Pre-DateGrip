export interface AbortRegistrable {
  register(id: string, controller: AbortController): void;
  abort(id: string): void;
  unregister(id: string): void;
}
