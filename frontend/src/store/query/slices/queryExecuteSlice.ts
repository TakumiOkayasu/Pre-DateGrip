import { executeAsyncWithPolling, toQueryResult } from '../helpers/asyncPolling';
import { endExecution, failExecution, startExecution } from '../helpers/executionState';
import type { AbortRegistrable } from '../interfaces/AbortRegistrable';
import type { Executable } from '../interfaces/Executable';
import type { HistoryRecordable } from '../interfaces/HistoryRecordable';
import type { QueryBridgeable } from '../interfaces/QueryBridgeable';
import type { GetState, SetState } from '../types';

interface ExecuteSliceDeps {
  bridge: QueryBridgeable;
  history: HistoryRecordable;
  abort: AbortRegistrable;
}

export function createExecuteSlice(
  set: SetState,
  get: GetState,
  deps: ExecuteSliceDeps
): Executable {
  const { bridge, history, abort } = deps;

  // W1修正: DRY — 共通の execute + history 記録関数
  async function executeAndRecord(id: string, connectionId: string, sql: string): Promise<void> {
    const controller = new AbortController();
    abort.register(id, controller);

    set((state) => startExecution(state, id));

    try {
      const result = await executeAsyncWithPolling(bridge, connectionId, sql, controller.signal);
      const { queryResult, totalAffectedRows, totalExecutionTimeMs } = toQueryResult(result);

      set((state) => ({
        ...endExecution(state, id),
        results: { ...state.results, [id]: queryResult },
      }));

      history.addHistory({
        sql,
        connectionId,
        timestamp: new Date(),
        executionTimeMs: totalExecutionTimeMs,
        affectedRows: totalAffectedRows,
        success: true,
        isFavorite: false,
      });
    } catch (error) {
      if (error instanceof DOMException && error.name === 'AbortError') {
        set((state) => endExecution(state, id));
        return;
      }
      const errorMessage = error instanceof Error ? error.message : 'Query execution failed';

      history.addHistory({
        sql,
        connectionId,
        timestamp: new Date(),
        executionTimeMs: 0,
        affectedRows: 0,
        success: false,
        errorMessage,
        isFavorite: false,
      });

      set((state) => failExecution(state, id, errorMessage));
    } finally {
      abort.unregister(id);
    }
  }

  return {
    executeQuery: async (id, connectionId) => {
      const query = get().queries.find((q) => q.id === id);
      if (!query || !query.content.trim()) return;
      await executeAndRecord(id, connectionId, query.content);
    },

    executeSelectedText: async (id, connectionId, selectedText) => {
      if (!selectedText.trim()) return;
      await executeAndRecord(id, connectionId, selectedText);
    },

    cancelQuery: async (connectionId) => {
      const { executingQueryIds, activeQueryId } = get();
      try {
        // Two-phase cancel (defence-in-depth):
        // 1) abort.abort — immediately stops each polling loop (UI responsiveness)
        // 2) bridge.cancelQuery — connection-level backend cancel (catches queries
        //    not yet in polling or managed outside AbortController)
        // Both are idempotent; the per-query cancelAsyncQuery fired by AbortError
        // arrives after bridge.cancelQuery and is a no-op on an already-cancelled query.
        for (const id of executingQueryIds) {
          abort.abort(id);
        }
        await bridge.cancelQuery(connectionId);
      } catch (error) {
        set((state) => ({
          errors: activeQueryId
            ? {
                ...state.errors,
                [activeQueryId]: error instanceof Error ? error.message : 'Failed to cancel query',
              }
            : state.errors,
        }));
      }
    },
  };
}
