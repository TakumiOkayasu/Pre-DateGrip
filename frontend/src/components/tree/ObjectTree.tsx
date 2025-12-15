import { useConnectionStore } from '../../store/connectionStore';
import { ConnectionTreeSection } from './ConnectionTreeSection';
import styles from './ObjectTree.module.css';

interface ObjectTreeProps {
  filter: string;
  onTableOpen?: (tableName: string, tableType: 'table' | 'view', connectionId?: string) => void;
}

export function ObjectTree({ filter, onTableOpen }: ObjectTreeProps) {
  const { connections } = useConnectionStore();

  // Get all active connections
  const activeConnections = connections.filter((c) => c.isActive);

  if (activeConnections.length === 0) {
    return <div className={styles.noConnection}>No active connection</div>;
  }

  return (
    <div className={styles.container}>
      {activeConnections.map((connection) => (
        <ConnectionTreeSection
          key={connection.id}
          connection={connection}
          filter={filter}
          onTableOpen={onTableOpen}
        />
      ))}
    </div>
  );
}
