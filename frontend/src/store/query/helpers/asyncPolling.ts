import type { AsyncColumn, AsyncPollResult, Column, QueryResult } from '../../../types';
import type { QueryBridgeable } from '../interfaces/QueryBridgeable';

function mapAsyncColumn(c: AsyncColumn): Column {
  return {
    name: c.name,
    type: c.type,
    size: 0,
    nullable: true,
    isPrimaryKey: false,
    comment: c.comment,
  };
}

const DEFAULT_QUERY_TIMEOUT_MS = 5 * 60 * 1000;
const POLL_INTERVAL_MS = 100;

export async function executeAsyncWithPolling(
  bridge: QueryBridgeable,
  connectionId: string,
  sql: string,
  signal?: AbortSignal
): Promise<AsyncPollResult> {
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
      if (result.multipleResults && result.results) {
        return {
          multipleResults: true,
          results: result.results,
        };
      }
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

export function toQueryResult(result: AsyncPollResult): {
  queryResult: QueryResult;
  totalAffectedRows: number;
  totalExecutionTimeMs: number;
} {
  if (result.multipleResults) {
    return {
      queryResult: {
        multipleResults: true,
        results: result.results.map((r) => ({
          statement: r.statement,
          data: {
            columns: r.data.columns.map(mapAsyncColumn),
            rows: r.data.rows,
            affectedRows: r.data.affectedRows,
            executionTimeMs: r.data.executionTimeMs,
          },
        })),
      },
      totalAffectedRows: result.results.reduce((sum, r) => sum + r.data.affectedRows, 0),
      totalExecutionTimeMs: result.results.reduce((sum, r) => sum + r.data.executionTimeMs, 0),
    };
  }

  return {
    queryResult: {
      columns: result.columns.map(mapAsyncColumn),
      rows: result.rows,
      affectedRows: result.affectedRows,
      executionTimeMs: result.executionTimeMs,
    },
    totalAffectedRows: result.affectedRows,
    totalExecutionTimeMs: result.executionTimeMs,
  };
}
