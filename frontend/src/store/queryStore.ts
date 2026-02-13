import { create } from 'zustand';
import { useShallow } from 'zustand/react/shallow';
import { bridge } from '../api/bridge';
import type { Query, QueryResult, ResultSet } from '../types';
import { log } from '../utils/logger';
import { useHistoryStore } from './historyStore';

export interface QueryState {
  queries: Query[];
  activeQueryId: string | null;
  results: Record<string, QueryResult>;
  /** Per-query executing state (each tab is an independent instance) */
  executingQueryIds: Set<string>;
  /** Per-query error state (each tab is an independent instance) */
  errors: Record<string, string | null>;
  /** @deprecated Use executingQueryIds instead. Kept for toolbar compatibility. */
  isExecuting: boolean;

  addQuery: (connectionId?: string | null) => void;
  removeQuery: (id: string) => void;
  updateQuery: (id: string, content: string) => void;
  renameQuery: (id: string, name: string) => void;
  setActive: (id: string | null) => void;
  executeQuery: (id: string, connectionId: string) => Promise<void>;
  executeSelectedText: (id: string, connectionId: string, selectedText: string) => Promise<void>;
  cancelQuery: (connectionId: string) => Promise<void>;
  formatQuery: (id: string) => Promise<void>;
  clearError: (id?: string) => void;
  openTableData: (
    connectionId: string,
    tableName: string,
    whereClause?: string,
    logicalName?: string
  ) => Promise<void>;
  applyWhereFilter: (id: string, connectionId: string, whereClause: string) => Promise<void>;
  refreshDataView: (id: string, connectionId: string) => Promise<void>;
  saveToFile: (id: string) => Promise<void>;
  loadFromFile: (id: string) => Promise<void>;
  openERDiagram: (name: string) => string;
}

let queryCounter = 0;

// Row limit for data view (table open) to prevent OOM on large tables
const DATA_VIEW_ROW_LIMIT = 10000;

// Default query timeout (5 minutes)
const DEFAULT_QUERY_TIMEOUT_MS = 5 * 60 * 1000;
const POLL_INTERVAL_MS = 100;

// Per-query AbortControllers for cancelling polling loops on tab close
const abortControllers = new Map<string, AbortController>();

// Execute a query asynchronously with polling (non-blocking for UI)
async function executeAsyncWithPolling(
  connectionId: string,
  sql: string,
  signal?: AbortSignal
): Promise<{
  columns: { name: string; type: string; comment?: string }[];
  rows: string[][];
  affectedRows: number;
  executionTimeMs: number;
}> {
  const { queryId } = await bridge.executeAsyncQuery(connectionId, sql);

  const startTime = Date.now();
  while (true) {
    if (signal?.aborted) {
      await bridge.cancelAsyncQuery(queryId).catch(() => {});
      throw new DOMException('Query cancelled', 'AbortError');
    }

    if (Date.now() - startTime > DEFAULT_QUERY_TIMEOUT_MS) {
      try {
        await bridge.cancelAsyncQuery(queryId);
      } catch {
        // Ignore cancel errors
      }
      throw new Error('Query execution timed out after 5 minutes');
    }

    const result = await bridge.getAsyncQueryResult(queryId);

    if (result.status === 'completed') {
      if (!result.columns || !result.rows) {
        throw new Error('Invalid query result: missing columns or rows');
      }
      return {
        columns: result.columns,
        rows: result.rows,
        affectedRows: result.affectedRows ?? 0,
        executionTimeMs: result.executionTimeMs ?? 0,
      };
    } else if (result.status === 'failed') {
      throw new Error(result.error || 'Query execution failed');
    } else if (result.status === 'cancelled') {
      throw new Error('Query was cancelled');
    }

    await new Promise((resolve) => setTimeout(resolve, POLL_INTERVAL_MS));
  }
}

// Fetch table data with column comments (DRY: used by openTableData, applyWhereFilter, refreshDataView)
async function fetchTableWithComments(
  connectionId: string,
  tableName: string,
  sql: string,
  signal?: AbortSignal
): Promise<ResultSet> {
  const [columnDefinitions, result] = await Promise.all([
    bridge.getColumns(connectionId, tableName),
    executeAsyncWithPolling(connectionId, sql, signal),
  ]);

  const commentMap = new Map(columnDefinitions.map((col) => [col.name, col.comment]));

  const isTruncated = result.rows.length > DATA_VIEW_ROW_LIMIT;
  const displayRows = isTruncated ? result.rows.slice(0, DATA_VIEW_ROW_LIMIT) : result.rows;

  return {
    columns: result.columns.map((c) => ({
      name: c.name,
      type: c.type,
      size: 0,
      nullable: true,
      isPrimaryKey: false,
      comment: commentMap.get(c.name) || undefined,
    })),
    rows: displayRows,
    affectedRows: result.affectedRows,
    executionTimeMs: result.executionTimeMs,
    truncated: isTruncated,
  };
}

// Per-query state transition helpers (DRY: used by all async operations)
function startExecution(state: QueryState, id: string): Partial<QueryState> {
  const newExecuting = new Set(state.executingQueryIds).add(id);
  return {
    executingQueryIds: newExecuting,
    errors: { ...state.errors, [id]: null },
    isExecuting: true,
  };
}

function endExecution(state: QueryState, id: string): Partial<QueryState> {
  const newExecuting = new Set(state.executingQueryIds);
  newExecuting.delete(id);
  return {
    executingQueryIds: newExecuting,
    isExecuting: newExecuting.size > 0,
  };
}

function failExecution(state: QueryState, id: string, errorMessage: string): Partial<QueryState> {
  return {
    ...endExecution(state, id),
    errors: { ...state.errors, [id]: errorMessage },
  };
}

export const useQueryStore = create<QueryState>((set, get) => ({
  queries: [],
  activeQueryId: null,
  results: {},
  executingQueryIds: new Set<string>(),
  errors: {},
  isExecuting: false,

  addQuery: (connectionId = null) => {
    const id = `query-${++queryCounter}`;
    const newQuery: Query = {
      id,
      name: `Query ${queryCounter}`,
      content: '',
      connectionId,
      isDirty: false,
    };

    set((state) => ({
      queries: [...state.queries, newQuery],
      activeQueryId: id,
    }));
  },

  removeQuery: (id) => {
    // Abort any in-flight polling loop (triggers backend cancel via AbortSignal)
    const controller = abortControllers.get(id);
    if (controller) {
      controller.abort();
      abortControllers.delete(id);
    }

    const { queries, activeQueryId, results, errors, executingQueryIds } = get();
    const index = queries.findIndex((q) => q.id === id);
    const newQueries = queries.filter((q) => q.id !== id);

    // Update results (remove the entry for the deleted query)
    const { [id]: _, ...newResults } = results;

    // Clean up per-query error
    const { [id]: _err, ...newErrors } = errors;

    // Clean up per-query executing state
    const newExecuting = new Set(executingQueryIds);
    newExecuting.delete(id);

    // Determine new active query
    let newActiveId: string | null = null;
    if (activeQueryId === id && newQueries.length > 0) {
      const newIndex = Math.min(index, newQueries.length - 1);
      newActiveId = newQueries[newIndex].id;
    } else if (activeQueryId !== id) {
      newActiveId = activeQueryId;
    }

    set({
      queries: newQueries,
      activeQueryId: newActiveId,
      results: newResults,
      errors: newErrors,
      executingQueryIds: newExecuting,
      isExecuting: newExecuting.size > 0,
    });
  },

  updateQuery: (id, content) => {
    set((state) => ({
      queries: state.queries.map((q) => (q.id === id ? { ...q, content, isDirty: true } : q)),
    }));
  },

  renameQuery: (id, name) => {
    set((state) => ({
      queries: state.queries.map((q) => (q.id === id ? { ...q, name } : q)),
    }));
  },

  setActive: (id) => {
    set({ activeQueryId: id });
  },

  executeQuery: async (id, connectionId) => {
    const query = get().queries.find((q) => q.id === id);
    if (!query || !query.content.trim()) return;

    const controller = new AbortController();
    abortControllers.set(id, controller);

    set((state) => startExecution(state, id));

    try {
      const result = await executeAsyncWithPolling(connectionId, query.content, controller.signal);

      if (!result.columns || !result.rows) {
        throw new Error('Invalid query result: missing columns or rows');
      }

      const queryResult: QueryResult = {
        columns: result.columns.map((c) => ({
          name: c.name,
          type: c.type,
          size: 0,
          nullable: true,
          isPrimaryKey: false,
          comment: c.comment,
        })),
        rows: result.rows,
        affectedRows: result.affectedRows,
        executionTimeMs: result.executionTimeMs,
      };

      set((state) => ({
        ...endExecution(state, id),
        results: { ...state.results, [id]: queryResult },
      }));

      useHistoryStore.getState().addHistory({
        sql: query.content,
        connectionId,
        timestamp: new Date(),
        executionTimeMs: result.executionTimeMs,
        affectedRows: result.affectedRows,
        success: true,
        isFavorite: false,
      });
    } catch (error) {
      if (error instanceof DOMException && error.name === 'AbortError') {
        set((state) => endExecution(state, id));
        return;
      }
      const errorMessage = error instanceof Error ? error.message : 'Query execution failed';

      useHistoryStore.getState().addHistory({
        sql: query.content,
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
      abortControllers.delete(id);
    }
  },

  executeSelectedText: async (id, connectionId, selectedText) => {
    if (!selectedText.trim()) return;

    const controller = new AbortController();
    abortControllers.set(id, controller);
    const { signal } = controller;

    set((state) => startExecution(state, id));

    try {
      // Start async query execution
      const { queryId } = await bridge.executeAsyncQuery(connectionId, selectedText);

      // Poll for results
      const pollInterval = 100; // Poll every 100ms
      const maxPollTime = DEFAULT_QUERY_TIMEOUT_MS;
      const startTime = Date.now();

      while (true) {
        // Check if aborted (tab closed)
        if (signal.aborted) {
          await bridge.cancelAsyncQuery(queryId).catch(() => {});
          throw new DOMException('Query cancelled', 'AbortError');
        }

        // Check timeout
        if (Date.now() - startTime > maxPollTime) {
          // Try to cancel the query before throwing
          try {
            await bridge.cancelAsyncQuery(queryId);
          } catch {
            // Ignore cancel errors
          }
          throw new Error('Query execution timed out after 5 minutes');
        }

        const result = await bridge.getAsyncQueryResult(queryId);

        if (result.status === 'completed') {
          // Query completed successfully
          let queryResult: QueryResult;
          let totalAffectedRows = 0;
          let totalExecutionTimeMs = 0;

          if (result.multipleResults && result.results) {
            // Multiple results format
            queryResult = {
              multipleResults: true,
              results: result.results.map((r) => ({
                statement: r.statement,
                data: {
                  columns: r.data.columns.map((c) => ({
                    name: c.name,
                    type: c.type,
                    size: 0,
                    nullable: true,
                    isPrimaryKey: false,
                  })),
                  rows: r.data.rows,
                  affectedRows: r.data.affectedRows,
                  executionTimeMs: r.data.executionTimeMs,
                },
              })),
            };
            totalAffectedRows = result.results.reduce((sum, r) => sum + r.data.affectedRows, 0);
            totalExecutionTimeMs = result.results.reduce(
              (sum, r) => sum + r.data.executionTimeMs,
              0
            );
          } else {
            // Single result format
            if (!result.columns || !result.rows) {
              throw new Error('Invalid query result: missing columns or rows');
            }

            queryResult = {
              columns: result.columns.map((c) => ({
                name: c.name,
                type: c.type,
                size: 0,
                nullable: true,
                isPrimaryKey: false,
                comment: c.comment,
              })),
              rows: result.rows,
              affectedRows: result.affectedRows ?? 0,
              executionTimeMs: result.executionTimeMs ?? 0,
            };
            totalAffectedRows = result.affectedRows ?? 0;
            totalExecutionTimeMs = result.executionTimeMs ?? 0;
          }

          set((state) => ({
            ...endExecution(state, id),
            results: { ...state.results, [id]: queryResult },
          }));

          useHistoryStore.getState().addHistory({
            sql: selectedText,
            connectionId,
            timestamp: new Date(),
            executionTimeMs: totalExecutionTimeMs,
            affectedRows: totalAffectedRows,
            success: true,
            isFavorite: false,
          });
          break;
        } else if (result.status === 'failed') {
          throw new Error(result.error || 'Query execution failed');
        } else if (result.status === 'cancelled') {
          throw new Error('Query was cancelled');
        }

        // Status is 'pending' or 'running' - wait before polling again
        await new Promise((resolve) => setTimeout(resolve, pollInterval));
      }
    } catch (error) {
      if (error instanceof DOMException && error.name === 'AbortError') {
        set((state) => endExecution(state, id));
        return;
      }
      const errorMessage = error instanceof Error ? error.message : 'Query execution failed';

      useHistoryStore.getState().addHistory({
        sql: selectedText,
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
      abortControllers.delete(id);
    }
  },

  cancelQuery: async (connectionId) => {
    try {
      await bridge.cancelQuery(connectionId);
      set({
        executingQueryIds: new Set<string>(),
        isExecuting: false,
      });
    } catch (error) {
      const activeId = get().activeQueryId;
      set((state) => ({
        errors: activeId
          ? {
              ...state.errors,
              [activeId]: error instanceof Error ? error.message : 'Failed to cancel query',
            }
          : state.errors,
      }));
    }
  },

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

  openTableData: async (connectionId, tableName, whereClause, logicalName) => {
    log.info(
      `[QueryStore] openTableData called for table: ${tableName}, connection: ${connectionId}${whereClause ? `, WHERE: ${whereClause}` : ''}`
    );

    // When whereClause is specified, always create a new tab (for related row navigation)
    if (!whereClause) {
      // Check if tab for this table already exists
      const existingQuery = get().queries.find(
        (q) => q.sourceTable === tableName && q.connectionId === connectionId && q.isDataView
      );

      if (existingQuery) {
        log.debug(
          `[QueryStore] Existing tab found for ${tableName}, activating: ${existingQuery.id}`
        );
        // Just activate the existing tab
        set({ activeQueryId: existingQuery.id });
        return;
      }
    }

    const id = `query-${++queryCounter}`;
    const sql = whereClause
      ? `SELECT TOP ${DATA_VIEW_ROW_LIMIT + 1} * FROM ${tableName} WHERE ${whereClause}`
      : `SELECT TOP ${DATA_VIEW_ROW_LIMIT + 1} * FROM ${tableName}`;
    const tabName = whereClause ? `${tableName} (フィルタ済)` : tableName;
    const newQuery: Query = {
      id,
      name: tabName,
      content: sql,
      connectionId,
      isDirty: false,
      sourceTable: tableName,
      isDataView: true,
      useServerSideRowModel: false, // Use client-side model (server-side requires Enterprise license)
      logicalName,
    };

    log.info(`[QueryStore] Creating new query tab: ${id} for table ${tableName}`);

    const controller = new AbortController();
    abortControllers.set(id, controller);

    set((state) => ({
      ...startExecution(state, id),
      queries: [...state.queries, newQuery],
      activeQueryId: id,
    }));

    try {
      log.debug(`[QueryStore] Fetching table data for ${tableName}`);
      const fetchStart = performance.now();

      const resultSet = await fetchTableWithComments(
        connectionId,
        tableName,
        sql,
        controller.signal
      );

      set((state) => ({
        ...endExecution(state, id),
        results: { ...state.results, [id]: resultSet },
      }));

      log.info(
        `[QueryStore] Loaded ${resultSet.rows.length} rows with ${resultSet.columns.length} columns in ${(performance.now() - fetchStart).toFixed(2)}ms`
      );
    } catch (error) {
      if (error instanceof DOMException && error.name === 'AbortError') {
        set((state) => endExecution(state, id));
        return;
      }
      log.error(`[QueryStore] Failed to fetch table data: ${error}`);
      set((state) =>
        failExecution(
          state,
          id,
          error instanceof Error ? error.message : 'Failed to load table data'
        )
      );
    } finally {
      abortControllers.delete(id);
    }
  },

  applyWhereFilter: async (id, connectionId, whereClause) => {
    const query = get().queries.find((q) => q.id === id);
    if (!query?.sourceTable) return;

    const baseSql = `SELECT TOP ${DATA_VIEW_ROW_LIMIT + 1} * FROM ${query.sourceTable}`;
    const sql = whereClause.trim() ? `${baseSql} WHERE ${whereClause}` : baseSql;

    const controller = new AbortController();
    abortControllers.set(id, controller);

    set((state) => ({
      ...startExecution(state, id),
      queries: state.queries.map((q) => (q.id === id ? { ...q, content: sql, isDirty: true } : q)),
    }));

    try {
      const resultSet = await fetchTableWithComments(
        connectionId,
        query.sourceTable,
        sql,
        controller.signal
      );

      set((state) => ({
        ...endExecution(state, id),
        results: { ...state.results, [id]: resultSet },
      }));
    } catch (error) {
      if (error instanceof DOMException && error.name === 'AbortError') {
        set((state) => endExecution(state, id));
        return;
      }
      set((state) =>
        failExecution(state, id, error instanceof Error ? error.message : 'Failed to apply filter')
      );
    } finally {
      abortControllers.delete(id);
    }
  },

  refreshDataView: async (id, connectionId) => {
    const query = get().queries.find((q) => q.id === id);
    if (!query?.sourceTable) return;

    log.info(`[QueryStore] Refreshing data view: ${query.sourceTable}`);

    const controller = new AbortController();
    abortControllers.set(id, controller);

    set((state) => startExecution(state, id));

    try {
      const resultSet = await fetchTableWithComments(
        connectionId,
        query.sourceTable,
        query.content,
        controller.signal
      );

      set((state) => ({
        ...endExecution(state, id),
        results: { ...state.results, [id]: resultSet },
      }));

      log.info(`[QueryStore] Data view refreshed: ${resultSet.rows.length} rows`);
    } catch (error) {
      if (error instanceof DOMException && error.name === 'AbortError') {
        set((state) => endExecution(state, id));
        return;
      }
      set((state) =>
        failExecution(
          state,
          id,
          error instanceof Error ? error.message : 'データの更新に失敗しました'
        )
      );
    } finally {
      abortControllers.delete(id);
    }
  },

  saveToFile: async (id) => {
    const query = get().queries.find((q) => q.id === id);
    if (!query || !query.content.trim()) return;

    try {
      const defaultFileName = query.filePath
        ? query.filePath.split('\\').pop()?.split('/').pop()
        : `${query.name}.sql`;

      const result = await bridge.saveQueryToFile(query.content, defaultFileName);

      log.info(`[QueryStore] Saved query to file: ${result.filePath}`);

      set((state) => ({
        queries: state.queries.map((q) =>
          q.id === id ? { ...q, filePath: result.filePath, isDirty: false } : q
        ),
      }));
    } catch (error) {
      // User cancelled or error occurred
      const message = error instanceof Error ? error.message : 'Failed to save file';
      if (!message.includes('cancelled')) {
        set((state) => ({ errors: { ...state.errors, [id]: message } }));
      }
    }
  },

  loadFromFile: async (id) => {
    try {
      const result = await bridge.loadQueryFromFile();

      log.info(`[QueryStore] Loaded query from file: ${result.filePath}`);

      set((state) => ({
        queries: state.queries.map((q) =>
          q.id === id
            ? { ...q, content: result.content, filePath: result.filePath, isDirty: false }
            : q
        ),
      }));
    } catch (error) {
      // User cancelled or error occurred
      const message = error instanceof Error ? error.message : 'Failed to load file';
      if (!message.includes('cancelled')) {
        set((state) => ({ errors: { ...state.errors, [id]: message } }));
      }
    }
  },

  openERDiagram: (name) => {
    // 同名のER図タブが既にあればアクティブ化
    const existing = get().queries.find((q) => q.isERDiagram && q.name === name);
    if (existing) {
      set({ activeQueryId: existing.id });
      return existing.id;
    }

    const id = `query-${++queryCounter}`;
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
}));

// Optimized selectors to prevent unnecessary re-renders
export const useQueries = () => useQueryStore(useShallow((state) => state.queries));

export const useActiveQuery = () =>
  useQueryStore((state) => {
    const query = state.queries.find((q) => q.id === state.activeQueryId);
    return query ?? null;
  });

export const useIsActiveDataView = () =>
  useQueryStore(
    (state) => state.queries.find((q) => q.id === state.activeQueryId)?.isDataView === true
  );

export const useIsActiveERDiagram = () =>
  useQueryStore(
    (state) => state.queries.find((q) => q.id === state.activeQueryId)?.isERDiagram === true
  );

export const useQueryById = (queryId: string | null | undefined) =>
  useQueryStore((state) => (queryId ? state.queries.find((q) => q.id === queryId) : undefined));

export const useQueryResult = (queryId: string | null | undefined) =>
  useQueryStore((state) => (queryId ? (state.results[queryId] ?? null) : null));

/** Per-query error selector */
export const useQueryError = (queryId: string | null | undefined) =>
  useQueryStore((state) => (queryId ? (state.errors[queryId] ?? null) : null));

/** Per-query executing state selector */
export const useIsQueryExecuting = (queryId: string | null | undefined) =>
  useQueryStore((state) => (queryId ? state.executingQueryIds.has(queryId) : false));

export const useQueryActions = () =>
  useQueryStore(
    useShallow((state) => ({
      addQuery: state.addQuery,
      removeQuery: state.removeQuery,
      updateQuery: state.updateQuery,
      renameQuery: state.renameQuery,
      setActive: state.setActive,
      executeQuery: state.executeQuery,
      executeSelectedText: state.executeSelectedText,
      cancelQuery: state.cancelQuery,
      formatQuery: state.formatQuery,
      clearError: state.clearError,
      openTableData: state.openTableData,
      applyWhereFilter: state.applyWhereFilter,
      refreshDataView: state.refreshDataView,
      saveToFile: state.saveToFile,
      loadFromFile: state.loadFromFile,
      openERDiagram: state.openERDiagram,
    }))
  );
