import type { QueryState } from '../types';

let queryCounter = 0;

export function generateQueryId(): string {
  return `query-${++queryCounter}`;
}

export function getQueryCounter(): number {
  return queryCounter;
}

/** Reset counter for tests / HMR. Not for production use. */
export function resetQueryCounter(): void {
  queryCounter = 0;
}

export function startExecution(state: QueryState, id: string): Partial<QueryState> {
  const newExecuting = new Set(state.executingQueryIds).add(id);
  return {
    executingQueryIds: newExecuting,
    errors: { ...state.errors, [id]: null },
    isExecuting: true,
  };
}

export function endExecution(state: QueryState, id: string): Partial<QueryState> {
  const newExecuting = new Set(state.executingQueryIds);
  newExecuting.delete(id);
  return {
    executingQueryIds: newExecuting,
    isExecuting: newExecuting.size > 0,
  };
}

export function failExecution(
  state: QueryState,
  id: string,
  errorMessage: string
): Partial<QueryState> {
  return {
    ...endExecution(state, id),
    errors: { ...state.errors, [id]: errorMessage },
  };
}
