import type { Column } from '../../../types';

export interface ColumnBridgeable {
  getColumns(connectionId: string, table: string): Promise<Column[]>;
}
