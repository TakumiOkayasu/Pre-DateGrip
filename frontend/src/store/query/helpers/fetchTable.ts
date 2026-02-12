import type { ResultSet } from '../../../types';
import type { ColumnBridgeable } from '../interfaces/ColumnBridgeable';
import type { QueryBridgeable } from '../interfaces/QueryBridgeable';
import { executeAsyncWithPolling } from './asyncPolling';

export const DATA_VIEW_ROW_LIMIT = 10000;

export async function fetchTableWithComments(
  bridge: QueryBridgeable & ColumnBridgeable,
  connectionId: string,
  tableName: string,
  sql: string,
  signal?: AbortSignal
): Promise<ResultSet> {
  const [columnDefinitions, pollResult] = await Promise.all([
    bridge.getColumns(connectionId, tableName),
    executeAsyncWithPolling(bridge, connectionId, sql, signal),
  ]);

  if (pollResult.multipleResults) {
    throw new Error('Unexpected multiple results for data view query');
  }
  const result = pollResult;

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
