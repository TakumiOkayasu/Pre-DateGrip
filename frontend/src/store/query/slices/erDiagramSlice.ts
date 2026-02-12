import type { Query } from '../../../types';
import { generateQueryId } from '../helpers/executionState';
import type { ERDiagrammable } from '../interfaces/ERDiagrammable';
import type { GetState, SetState } from '../types';

export function createERDiagramSlice(set: SetState, get: GetState): ERDiagrammable {
  return {
    openERDiagram: (name) => {
      // 同名のER図タブが既にあればアクティブ化
      const existing = get().queries.find((q) => q.isERDiagram && q.name === name);
      if (existing) {
        set({ activeQueryId: existing.id });
        return existing.id;
      }

      const id = generateQueryId();
      const newQuery: Query = {
        id,
        name,
        content: '',
        connectionId: null,
        isDirty: false,
        isERDiagram: true,
      };

      set((state) => ({
        queries: [...state.queries, newQuery],
        activeQueryId: id,
      }));

      return id;
    },
  };
}
