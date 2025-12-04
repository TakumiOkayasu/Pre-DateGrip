import type { ColDef, GridReadyEvent } from 'ag-grid-community';
import { AgGridReact } from 'ag-grid-react';
import { useCallback, useMemo, useRef } from 'react';
import type { ReferencingForeignKeyInfo } from '../../../types';
import 'ag-grid-community/styles/ag-grid.css';
import 'ag-grid-community/styles/ag-theme-alpine.css';
import styles from '../TableViewer.module.css';

interface ReferencingForeignKeysTabProps {
  referencingForeignKeys: ReferencingForeignKeyInfo[];
}

export function ReferencingForeignKeysTab({
  referencingForeignKeys,
}: ReferencingForeignKeysTabProps) {
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
        field: 'referencingTable',
        headerName: 'Referencing Table',
        flex: 1,
        minWidth: 150,
        sortable: true,
        filter: true,
      },
      {
        field: 'referencingColumns',
        headerName: 'Referencing Columns',
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
        field: 'columns',
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

  if (referencingForeignKeys.length === 0) {
    return <div className={styles.emptyState}>No referencing foreign keys found</div>;
  }

  return (
    <div className={`ag-theme-alpine-dark ${styles.gridContainer}`}>
      <AgGridReact
        ref={gridRef}
        columnDefs={columnDefs}
        rowData={referencingForeignKeys}
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
