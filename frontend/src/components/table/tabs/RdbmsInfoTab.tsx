import type { TableMetadata } from '../../../types';
import styles from '../TableViewer.module.css';

interface RdbmsInfoTabProps {
  metadata: TableMetadata | null;
}

export function RdbmsInfoTab({ metadata }: RdbmsInfoTabProps) {
  if (!metadata) {
    return <div className={styles.emptyState}>No metadata available</div>;
  }

  const infoItems = [
    { label: 'Schema', value: metadata.schema },
    { label: 'Name', value: metadata.name },
    { label: 'Type', value: metadata.type },
    { label: 'Row Count', value: metadata.rowCount.toLocaleString() },
    { label: 'Created At', value: metadata.createdAt || 'N/A' },
    { label: 'Modified At', value: metadata.modifiedAt || 'N/A' },
    { label: 'Owner', value: metadata.owner || 'N/A' },
    { label: 'Comment', value: metadata.comment || 'No comment' },
  ];

  return (
    <div style={{ padding: '16px', overflow: 'auto', height: '100%' }}>
      <table className={styles.infoTable}>
        <tbody>
          {infoItems.map((item) => (
            <tr key={item.label}>
              <th style={{ width: '200px' }}>{item.label}</th>
              <td>{item.value}</td>
            </tr>
          ))}
        </tbody>
      </table>
    </div>
  );
}
