import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';
import { useQueryStore } from '../../store/queryStore';

vi.mock('../../api/bridge', () => ({
  bridge: {
    executeAsyncQuery: vi.fn(),
    getAsyncQueryResult: vi.fn(),
    cancelAsyncQuery: vi.fn(),
    cancelQuery: vi.fn(),
    getColumns: vi.fn(),
    formatSQL: vi.fn(),
    saveQueryToFile: vi.fn(),
    loadQueryFromFile: vi.fn(),
  },
}));

import { bridge } from '../../api/bridge';

const mockedBridge = vi.mocked(bridge);

describe('cancelQuery + AbortRegistrable', () => {
  beforeEach(() => {
    useQueryStore.setState({
      queries: [],
      activeQueryId: null,
      results: {},
      executingQueryIds: new Set<string>(),
      errors: {},
      isExecuting: false,
    });
    vi.clearAllMocks();
  });

  afterEach(() => {
    vi.clearAllMocks();
  });

  it('should call bridge.cancelQuery with connectionId', async () => {
    mockedBridge.cancelQuery.mockResolvedValue(undefined);

    const { addQuery, cancelQuery } = useQueryStore.getState();
    addQuery('conn_1');

    await cancelQuery('conn_1');

    expect(mockedBridge.cancelQuery).toHaveBeenCalledWith('conn_1');
  });

  it('should set error on active query when cancelQuery fails', async () => {
    mockedBridge.cancelQuery.mockRejectedValue(new Error('Cancel failed'));

    const { addQuery, cancelQuery } = useQueryStore.getState();
    addQuery('conn_1');
    const queryId = useQueryStore.getState().queries[0].id;

    await cancelQuery('conn_1');

    const { errors } = useQueryStore.getState();
    expect(errors[queryId]).toBe('Cancel failed');
  });

  it('should abort in-flight query on removeQuery', async () => {
    const { addQuery, updateQuery, executeQuery, removeQuery } = useQueryStore.getState();
    addQuery('conn_1');
    const queryId = useQueryStore.getState().queries[0].id;
    updateQuery(queryId, 'SELECT WAITFOR DELAY');

    // Mock: executeAsyncQuery resolves, then getAsyncQueryResult returns pending repeatedly
    mockedBridge.executeAsyncQuery.mockResolvedValue({ queryId: 'async_1' });
    mockedBridge.getAsyncQueryResult.mockResolvedValue({
      queryId: 'async_1',
      status: 'pending' as const,
    });
    mockedBridge.cancelAsyncQuery.mockResolvedValue({ cancelled: true });

    // Start execution (don't await — it will keep polling 'pending')
    const execPromise = executeQuery(queryId, 'conn_1');

    // Wait for execution to start (polling loop is running)
    await vi.waitFor(() => {
      expect(useQueryStore.getState().executingQueryIds.has(queryId)).toBe(true);
    });

    // Remove tab while query is in-flight → triggers abort
    removeQuery(queryId);

    // The abort should cause executeQuery to resolve via AbortError
    await execPromise;

    const state = useQueryStore.getState();
    expect(state.queries).toHaveLength(0);
    expect(state.executingQueryIds.has(queryId)).toBe(false);
  });

  it('should handle cancelQuery when no active query', async () => {
    mockedBridge.cancelQuery.mockResolvedValue(undefined);

    useQueryStore.setState({ activeQueryId: null });
    const { cancelQuery } = useQueryStore.getState();

    await cancelQuery('conn_1');

    expect(mockedBridge.cancelQuery).toHaveBeenCalledWith('conn_1');
  });
});
