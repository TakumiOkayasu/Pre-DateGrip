import type { CellClassParams, ColDef, GridReadyEvent } from 'ag-grid-community';
import { AgGridReact } from 'ag-grid-react';
import { useCallback, useMemo, useRef, useState } from 'react';
import type { ResultSet } from '../../../types';
import 'ag-grid-community/styles/ag-grid.css';
import 'ag-grid-community/styles/ag-theme-alpine.css';
import styles from '../TableViewer.module.css';

interface DataTabProps {
  resultSet: ResultSet | null;
  whereClause: string;
  onWhereClauseChange: (value: string) => void;
  onApplyFilter: () => void;
  showLogicalNames: boolean;
}

export function DataTab({
  resultSet,
  whereClause,
  onWhereClauseChange,
  onApplyFilter,
  showLogicalNames,
}: DataTabProps) {
  const gridRef = useRef<AgGridReact>(null);
  const [, setGridReady] = useState(false);

  const columnDefs = useMemo<ColDef[]>(() => {
    if (!resultSet) return [];
    return resultSet.columns.map((col) => ({
      field: col.name,
      headerName: showLogicalNames ? col.name : col.name, // TODO: Use logical name when available
      headerTooltip: `${col.name} (${col.type})`,
      sortable: true,
      filter: true,
      resizable: true,
      cellClass: (params: CellClassParams) => {
        if (params.value === null || params.value === '') {
          return styles.nullCell;
        }
        return '';
      },
      valueFormatter: (params) => {
        if (params.value === null || params.value === undefined) {
          return 'NULL';
        }
        return params.value;
      },
    }));
  }, [resultSet, showLogicalNames]);

  const rowData = useMemo(() => {
    if (!resultSet) return [];
    return resultSet.rows.map((row, rowIndex) => {
      const obj: Record<string, string | null> = {
        __rowIndex: String(rowIndex + 1),
      };
      resultSet.columns.forEach((col, idx) => {
        const value = row[idx];
        obj[col.name] = value === '' || value === undefined ? null : value;
      });
      return obj;
    });
  }, [resultSet]);

  const onGridReady = useCallback((params: GridReadyEvent) => {
    params.api.sizeColumnsToFit();
    setGridReady(true);
  }, []);

  const handleWhereKeyDown = useCallback(
    (e: React.KeyboardEvent<HTMLInputElement>) => {
      if (e.key === 'Enter') {
        onApplyFilter();
      }
    },
    [onApplyFilter]
  );

  if (!resultSet) {
    return <div className={styles.emptyState}>No data loaded</div>;
  }

  return (
    <div style={{ height: '100%', display: 'flex', flexDirection: 'column' }}>
      {/* WHERE Filter Bar */}
      <div className={styles.filterBar}>
        <span className={styles.filterLabel}>WHERE</span>
        <input
          type="text"
          className={styles.filterInput}
          placeholder="e.g. id > 100 AND name LIKE '%test%'"
          value={whereClause}
          onChange={(e) => onWhereClauseChange(e.target.value)}
          onKeyDown={handleWhereKeyDown}
        />
        <button className={styles.filterButton} onClick={onApplyFilter}>
          Apply
        </button>
        <button
          className={styles.filterButton}
          onClick={() => {
            onWhereClauseChange('');
            onApplyFilter();
          }}
          disabled={!whereClause}
        >
          Clear
        </button>
      </div>

      {/* Data Grid */}
      <div className={`ag-theme-alpine-dark ${styles.gridContainer}`}>
        <AgGridReact
          ref={gridRef}
          columnDefs={columnDefs}
          rowData={rowData}
          defaultColDef={{
            flex: 1,
            minWidth: 100,
            sortable: true,
            filter: true,
            resizable: true,
          }}
          onGridReady={onGridReady}
          enableCellTextSelection={true}
          ensureDomOrder={true}
          animateRows={false}
          rowBuffer={20}
          rowSelection="multiple"
          suppressRowClickSelection={false}
          copyHeadersToClipboard={true}
          suppressCopyRowsToClipboard={false}
        />
      </div>

      {/* Status Bar */}
      <div className={styles.statusBar}>
        <span>{resultSet.rows.length} rows</span>
        <span>|</span>
        <span>{resultSet.executionTimeMs.toFixed(2)} ms</span>
      </div>
    </div>
  );
}
