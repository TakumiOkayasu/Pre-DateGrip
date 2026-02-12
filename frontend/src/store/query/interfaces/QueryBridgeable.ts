import type { AsyncQueryResultResponse } from '../../../types';

export interface QueryBridgeable {
  executeAsyncQuery(connectionId: string, sql: string): Promise<{ queryId: string }>;
  getAsyncQueryResult(queryId: string): Promise<AsyncQueryResultResponse>;
  cancelAsyncQuery(queryId: string): Promise<{ cancelled: boolean }>;
  cancelQuery(connectionId: string): Promise<void>;
}
