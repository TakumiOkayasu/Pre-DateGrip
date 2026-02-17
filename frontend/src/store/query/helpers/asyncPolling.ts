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

  try {
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
        if (result.multipleResults) {
          return {
            multipleResults: true,
            results: result.results,
          };
        }
        return {
          columns: result.columns,
          rows: result.rows,
          affectedRows: result.affectedRows,
          executionTimeMs: result.executionTimeMs,
        };
      } else if (result.status === 'failed') {
        throw new Error(result.error);
      } else if (result.status === 'cancelled') {
        throw new Error('Query was cancelled');
      }

      await new Promise((resolve) => setTimeout(resolve, POLL_INTERVAL_MS));
    }
  } finally {
    // Release backend memory for this query (single cleanup point).
    // Runs on all exit paths: success, failure, timeout, and abort.
    bridge.removeAsyncQuery(queryId).catch(() => {});
  }
}

const QUERY_ROW_LIMIT = 10_000;

function truncateRows(rows: string[][]): { rows: string[][]; truncated: boolean } {
  if (rows.length > QUERY_ROW_LIMIT) {
    return { rows: rows.slice(0, QUERY_ROW_LIMIT), truncated: true };
  }
  return { rows, truncated: false };
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
        results: result.results.map((r) => {
          const { rows, truncated } = truncateRows(r.data.rows);
          return {
            statement: r.statement,
            data: {
              columns: r.data.columns.map(mapAsyncColumn),
              rows,
              affectedRows: r.data.affectedRows,
              executionTimeMs: r.data.executionTimeMs,
              truncated,
            },
          };
        }),
      },
      totalAffectedRows: result.results.reduce((sum, r) => sum + r.data.affectedRows, 0),
      totalExecutionTimeMs: result.results.reduce((sum, r) => sum + r.data.executionTimeMs, 0),
    };
  }

  const { rows, truncated } = truncateRows(result.rows);
  return {
    queryResult: {
      columns: result.columns.map(mapAsyncColumn),
      rows,
      affectedRows: result.affectedRows,
      executionTimeMs: result.executionTimeMs,
      truncated,
    },
    totalAffectedRows: result.affectedRows,
    totalExecutionTimeMs: result.executionTimeMs,
  };
}
