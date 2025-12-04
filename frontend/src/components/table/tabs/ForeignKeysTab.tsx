import type { ColDef, GridReadyEvent } from 'ag-grid-community';
import { AgGridReact } from 'ag-grid-react';
import { useCallback, useMemo, useRef } from 'react';
import type { ForeignKeyInfo } from '../../../types';
import 'ag-grid-community/styles/ag-grid.css';
import 'ag-grid-community/styles/ag-theme-alpine.css';
import styles from '../TableViewer.module.css';

interface ForeignKeysTabProps {
  foreignKeys: ForeignKeyInfo[];
}

export function ForeignKeysTab({ foreignKeys }: ForeignKeysTabProps) {
  const gridRef = useRef<AgGridReact>(null);

  const columnDefs = useMemo<ColDef[]>(() => {
    return [
      {
        field: 'name',
        headerName: 'FK Name',
        flex: 1,
        minWidth: 200,
        sortable: true,
        filter: true,
      },
      {
        field: 'columns',
        headerName: 'Columns',
        flex: 1,
        minWidth: 150,
        sortable: true,
        valueFormatter: (params) => {
          if (Array.isArray(params.value)) {
            return params.value.join(', ');
          }
          return params.value;
        },
      },
      {
        field: 'referencedTable',
        headerName: 'Referenced Table',
        flex: 1,
        minWidth: 150,
        sortable: true,
        filter: true,
      },
      {
        field: 'referencedColumns',
        headerName: 'Referenced Columns',
        flex: 1,
        minWidth: 150,
        sortable: true,
        valueFormatter: (params) => {
          if (Array.isArray(params.value)) {
            return params.value.join(', ');
          }
          return params.value;
        },
      },
      {
        field: 'onDelete',
        headerName: 'On Delete',
        width: 100,
        sortable: true,
      },
      {
        field: 'onUpdate',
        headerName: 'On Update',
        width: 100,
        sortable: true,
      },
    ];
  }, []);

  const onGridReady = useCallback((params: GridReadyEvent) => {
    params.api.sizeColumnsToFit();
  }, []);

  if (foreignKeys.length === 0) {
    return <div className={styles.emptyState}>No foreign keys found</div>;
  }

  return (
    <div className={`ag-theme-alpine-dark ${styles.gridContainer}`}>
      <AgGridReact
        ref={gridRef}
        columnDefs={columnDefs}
        rowData={foreignKeys}
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
