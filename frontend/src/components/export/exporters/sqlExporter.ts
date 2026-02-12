import type { ResultSet } from '../../../types';
import type { Exportable, ExportOptions } from './types';

export const sqlExporter: Exportable = {
  generate(resultSet: ResultSet, options: ExportOptions): string {
    const { columns, rows } = resultSet;
    const { tableName } = options;

    const lines: string[] = [];
    for (const row of rows) {
      const values = row.map((val) => {
        if (val === null || val === '') return 'NULL';
        const escaped = String(val).replace(/'/g, "''");
        return `N'${escaped}'`;
      });
      lines.push(
        `INSERT INTO [${tableName}] ([${columns.map((c) => c.name).join('], [')}]) VALUES (${values.join(', ')});`
      );
    }
    return lines.join('\n');
  },
};
