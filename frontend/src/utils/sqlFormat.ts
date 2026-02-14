import { format } from 'sql-formatter';

export function formatSQL(sql: string): string {
  return format(sql, {
    language: 'transactsql',
    keywordCase: 'upper',
    indentStyle: 'standard',
    tabWidth: 4,
  });
}
