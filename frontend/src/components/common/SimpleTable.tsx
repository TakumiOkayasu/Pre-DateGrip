import styles from './SimpleTable.module.css';

interface Column {
  field: string;
  headerName: string;
  width?: number | string;
  formatter?: (value: unknown) => string;
}

interface SimpleTableProps {
  columns: Column[];
  data: Record<string, unknown>[];
}

export function SimpleTable({ columns, data }: SimpleTableProps) {
  return (
    <div className={styles.container}>
      <table className={styles.table}>
        <thead>
          <tr>
            {columns.map((col) => (
              <th key={col.field} style={{ width: col.width }}>
                {col.headerName}
              </th>
            ))}
          </tr>
        </thead>
        <tbody>
          {data.map((row, idx) => {
            // Generate unique key by combining index and first column value
            const firstField = columns[0]?.field ?? 'id';
            const rowKey = `${idx}-${String(row[firstField] ?? idx)}`;
            return (
              <tr key={rowKey}>
                {columns.map((col) => {
                  const value = row[col.field];
                  const displayValue = col.formatter ? col.formatter(value) : String(value ?? '');
                  return <td key={col.field}>{displayValue}</td>;
                })}
              </tr>
            );
          })}
        </tbody>
      </table>
    </div>
  );
}
