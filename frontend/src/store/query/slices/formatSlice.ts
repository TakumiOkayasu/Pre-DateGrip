import type { Formattable } from '../interfaces/Formattable';
import type { SqlBridgeable } from '../interfaces/SqlBridgeable';
import type { GetState, SetState } from '../types';

interface FormatSliceDeps {
  bridge: SqlBridgeable;
}

export function createFormatSlice(
  set: SetState,
  get: GetState,
  deps: FormatSliceDeps
): Formattable {
  const { bridge } = deps;

  return {
    formatQuery: async (id) => {
      const query = get().queries.find((q) => q.id === id);
      if (!query || !query.content.trim() || query.isDataView) return;

      try {
        const result = await bridge.formatSQL(query.content);
        set((state) => ({
          queries: state.queries.map((q) =>
            q.id === id ? { ...q, content: result.sql, isDirty: true } : q
          ),
        }));
      } catch (error) {
        set((state) => ({
          errors: {
            ...state.errors,
            [id]: error instanceof Error ? error.message : 'Failed to format SQL',
          },
        }));
      }
    },

    clearError: (id?) => {
      if (id) {
        set((state) => ({
          errors: { ...state.errors, [id]: null },
        }));
      } else {
        set({ errors: {} });
      }
    },
  };
}
