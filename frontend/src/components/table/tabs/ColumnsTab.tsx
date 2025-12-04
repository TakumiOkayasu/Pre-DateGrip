import type { ColDef, GridReadyEvent } from 'ag-grid-community';
import { AgGridReact } from 'ag-grid-react';
import { useCallback, useMemo, useRef } from 'react';
import type { Column } from '../../../types';
import 'ag-grid-community/styles/ag-grid.css';
import 'ag-grid-community/styles/ag-theme-alpine.css';
import styles from '../TableViewer.module.css';

interface ColumnsTabProps {
  columns: Column[];
  showLogicalNames: boolean;
}

export function ColumnsTab({ columns, showLogicalNames }: ColumnsTabProps) {
  const gridRef = useRef<AgGridReact>(null);

  const columnDefs = useMemo<ColDef[]>(() => {
    const defs: ColDef[] = [
      {
        field: 'ordinal',
        headerName: '#',
        width: 60,
        sortable: true,
      },
      {
        field: 'name',
        headerName: showLogicalNames ? 'Logical Name' : 'Column Name',
        flex: 1,
        minWidth: 150,
        sortable: true,
        filter: true,
      },
      {
        field: 'type',
        headerName: 'Data Type',
        width: 120,
        sortable: true,
        filter: true,
      },
      {
        field: 'size',
        headerName: 'Size',
        width: 80,
        sortable: true,
        valueFormatter: (params) => {
          if (params.value === 0 || params.value === -1) {
            return 'MAX';
          }
          return params.value;
        },
      },
      {
        field: 'nullable',
        headerName: 'Nullable',
        width: 90,
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
    return defs;
  }, [showLogicalNames]);

  const rowData = useMemo(() => {
    return columns.map((col, index) => ({
      ordinal: index + 1,
      name: col.name,
      type: col.type,
      size: col.size,
      nullable: col.nullable,
      isPrimaryKey: col.isPrimaryKey,
    }));
  }, [columns]);

  const onGridReady = useCallback((params: GridReadyEvent) => {
    params.api.sizeColumnsToFit();
  }, []);

  if (columns.length === 0) {
    return <div className={styles.emptyState}>No columns found</div>;
  }

  return (
    <div className={`ag-theme-alpine-dark ${styles.gridContainer}`}>
      <AgGridReact
        ref={gridRef}
        columnDefs={columnDefs}
        rowData={rowData}
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
