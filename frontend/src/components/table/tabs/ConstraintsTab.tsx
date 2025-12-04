import type { ColDef, GridReadyEvent } from 'ag-grid-community';
import { AgGridReact } from 'ag-grid-react';
import { useCallback, useMemo, useRef } from 'react';
import type { ConstraintInfo } from '../../../types';
import 'ag-grid-community/styles/ag-grid.css';
import 'ag-grid-community/styles/ag-theme-alpine.css';
import styles from '../TableViewer.module.css';

interface ConstraintsTabProps {
  constraints: ConstraintInfo[];
}

export function ConstraintsTab({ constraints }: ConstraintsTabProps) {
  const gridRef = useRef<AgGridReact>(null);

  const columnDefs = useMemo<ColDef[]>(() => {
    return [
      {
        field: 'name',
        headerName: 'Constraint Name',
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
        field: 'definition',
        headerName: 'Definition',
        flex: 2,
        minWidth: 200,
        sortable: true,
      },
    ];
  }, []);

  const onGridReady = useCallback((params: GridReadyEvent) => {
    params.api.sizeColumnsToFit();
  }, []);

  if (constraints.length === 0) {
    return <div className={styles.emptyState}>No constraints found</div>;
  }

  return (
    <div className={`ag-theme-alpine-dark ${styles.gridContainer}`}>
      <AgGridReact
        ref={gridRef}
        columnDefs={columnDefs}
        rowData={constraints}
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
