import type { ColDef, GridReadyEvent } from 'ag-grid-community';
import { AgGridReact } from 'ag-grid-react';
import { useCallback, useMemo, useRef } from 'react';
import type { IndexInfo } from '../../../types';
import 'ag-grid-community/styles/ag-grid.css';
import 'ag-grid-community/styles/ag-theme-alpine.css';
import styles from '../TableViewer.module.css';

interface IndexesTabProps {
  indexes: IndexInfo[];
}

export function IndexesTab({ indexes }: IndexesTabProps) {
  const gridRef = useRef<AgGridReact>(null);

  const columnDefs = useMemo<ColDef[]>(() => {
    return [
      {
        field: 'name',
        headerName: 'Index Name',
        flex: 1,
        minWidth: 200,
        sortable: true,
        filter: true,
      },
      {
        field: 'type',
        headerName: 'Type',
        width: 120,
        sortable: true,
        filter: true,
      },
      {
        field: 'columns',
        headerName: 'Columns',
        flex: 1,
        minWidth: 200,
        sortable: true,
        valueFormatter: (params) => {
          if (Array.isArray(params.value)) {
            return params.value.join(', ');
          }
          return params.value;
        },
      },
      {
        field: 'isUnique',
        headerName: 'Unique',
        width: 80,
        sortable: true,
        cellClass: (params) => (params.value ? styles.checkCell : ''),
        valueFormatter: (params) => (params.value ? '✓' : ''),
      },
      {
        field: 'isPrimaryKey',
        headerName: 'PK',
        width: 60,
        sortable: true,
        cellClass: (params) => (params.value ? styles.checkCell : ''),
        valueFormatter: (params) => (params.value ? '✓' : ''),
      },
    ];
  }, []);

  const onGridReady = useCallback((params: GridReadyEvent) => {
    params.api.sizeColumnsToFit();
  }, []);

  if (indexes.length === 0) {
    return <div className={styles.emptyState}>No indexes found</div>;
  }

  return (
    <div className={`ag-theme-alpine-dark ${styles.gridContainer}`}>
      <AgGridReact
        ref={gridRef}
        columnDefs={columnDefs}
        rowData={indexes}
        defaultColDef={{
          resizable: true,
        }}
        onGridReady={onGridReady}
        enableCellTextSelection={true}
        animateRows={false}
      />
    </div>
  );
}
