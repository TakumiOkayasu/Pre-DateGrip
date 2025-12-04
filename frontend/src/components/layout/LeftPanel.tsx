import { useCallback, useEffect, useState } from 'react';
import { bridge } from '../../api/bridge';
import { useConnectionStore } from '../../store/connectionStore';
import { useQueryActions } from '../../store/queryStore';
import { ObjectTree } from '../tree/ObjectTree';
import styles from './LeftPanel.module.css';

interface LeftPanelProps {
  width: number;
}

const DEFAULT_PAGE_SIZE = 1000;

export function LeftPanel({ width }: LeftPanelProps) {
  const [searchFilter, setSearchFilter] = useState('');
  const [pageSize, setPageSize] = useState(DEFAULT_PAGE_SIZE);
  const { activeConnectionId } = useConnectionStore();
  const { openTableData } = useQueryActions();

  // Load page size from settings
  useEffect(() => {
    const loadSettings = async () => {
      try {
        const settings = await bridge.getSettings();
        if (settings.grid?.defaultPageSize) {
          setPageSize(settings.grid.defaultPageSize);
        }
      } catch {
        // Use default if settings not available
      }
    };
    loadSettings();
  }, []);

  const handleTableOpen = useCallback(
    (tableName: string, _tableType: 'table' | 'view') => {
      if (activeConnectionId) {
        openTableData(activeConnectionId, tableName, pageSize);
      }
    },
    [activeConnectionId, openTableData, pageSize]
  );

  return (
    <div className={styles.container} style={{ width }}>
      <div className={styles.header}>
        <span>Database</span>
      </div>

      <div className={styles.searchBox}>
        <input
          type="text"
          placeholder="Search objects..."
          value={searchFilter}
          onChange={(e) => setSearchFilter(e.target.value)}
        />
      </div>

      <div className={styles.treeContainer}>
        <ObjectTree filter={searchFilter} onTableOpen={handleTableOpen} />
      </div>
    </div>
  );
}
