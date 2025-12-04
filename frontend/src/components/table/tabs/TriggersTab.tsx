import type { ColDef, GridReadyEvent } from 'ag-grid-community';
import { AgGridReact } from 'ag-grid-react';
import { useCallback, useMemo, useRef, useState } from 'react';
import type { TriggerInfo } from '../../../types';
import 'ag-grid-community/styles/ag-grid.css';
import 'ag-grid-community/styles/ag-theme-alpine.css';
import styles from '../TableViewer.module.css';

interface TriggersTabProps {
  triggers: TriggerInfo[];
}

export function TriggersTab({ triggers }: TriggersTabProps) {
  const gridRef = useRef<AgGridReact>(null);
  const [selectedTrigger, setSelectedTrigger] = useState<TriggerInfo | null>(null);

  const columnDefs = useMemo<ColDef[]>(() => {
    return [
      {
        field: 'name',
        headerName: 'Trigger Name',
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
        field: 'events',
        headerName: 'Events',
        width: 150,
        sortable: true,
        valueFormatter: (params) => {
          if (Array.isArray(params.value)) {
            return params.value.join(', ');
          }
          return params.value;
        },
      },
      {
        field: 'isEnabled',
        headerName: 'Enabled',
        width: 80,
        sortable: true,
        cellClass: (params) => (params.value ? styles.checkCell : ''),
        valueFormatter: (params) => (params.value ? '✓' : '✗'),
      },
    ];
  }, []);

  const onGridReady = useCallback((params: GridReadyEvent) => {
    params.api.sizeColumnsToFit();
  }, []);

  const onRowClicked = useCallback((event: { data: TriggerInfo }) => {
    setSelectedTrigger(event.data);
  }, []);

  if (triggers.length === 0) {
    return <div className={styles.emptyState}>No triggers found</div>;
  }

  return (
    <div style={{ height: '100%', display: 'flex', flexDirection: 'column' }}>
      <div className={`ag-theme-alpine-dark ${styles.gridContainer}`} style={{ flex: 1 }}>
        <AgGridReact
          ref={gridRef}
          columnDefs={columnDefs}
          rowData={triggers}
          defaultColDef={{
            resizable: true,
          }}
          onGridReady={onGridReady}
          onRowClicked={onRowClicked}
          enableCellTextSelection={true}
          animateRows={false}
          rowSelection="single"
        />
      </div>

      {/* Trigger Definition Panel */}
      {selectedTrigger && (
        <div
          className={styles.codeContainer}
          style={{ flex: 1, borderTop: '1px solid var(--border-color)' }}
        >
          <pre className={styles.code}>
            {selectedTrigger.definition || 'No definition available'}
          </pre>
        </div>
      )}
    </div>
  );
}
