import { useMemo, useCallback, useRef } from 'react'
import { AgGridReact } from 'ag-grid-react'
import type { ColDef, GridReadyEvent, CellClassParams } from 'ag-grid-community'
import { useQueryStore } from '../../store/queryStore'
import 'ag-grid-community/styles/ag-grid.css'
import 'ag-grid-community/styles/ag-theme-alpine.css'
import styles from './ResultGrid.module.css'

export function ResultGrid() {
  const { activeQueryId, results, isExecuting, error } = useQueryStore()
  const gridRef = useRef<AgGridReact>(null)

  const resultSet = activeQueryId ? results.get(activeQueryId) : null

  const columnDefs = useMemo<ColDef[]>(() => {
    if (!resultSet) return []
    return resultSet.columns.map((col) => ({
      field: col.name,
      headerName: col.name,
      headerTooltip: `${col.name} (${col.type})`,
      sortable: true,
      filter: true,
      resizable: true,
      cellClass: (params: CellClassParams) =>
        params.value === null || params.value === '' ? styles.nullCell : '',
      valueFormatter: (params) => {
        if (params.value === null || params.value === undefined) {
          return 'NULL'
        }
        return params.value
      },
    }))
  }, [resultSet])

  const rowData = useMemo(() => {
    if (!resultSet) return []
    return resultSet.rows.map((row, rowIndex) => {
      const obj: Record<string, string | null> = { __rowIndex: String(rowIndex + 1) }
      resultSet.columns.forEach((col, idx) => {
        const value = row[idx]
        obj[col.name] = value === '' || value === undefined ? null : value
      })
      return obj
    })
  }, [resultSet])

  const onGridReady = useCallback((params: GridReadyEvent) => {
    params.api.sizeColumnsToFit()
  }, [])

  if (isExecuting) {
    return (
      <div className={styles.message}>
        <span className={styles.spinner}>⏳</span>
        <span>Executing query...</span>
      </div>
    )
  }

  if (error) {
    return (
      <div className={`${styles.message} ${styles.error}`}>
        <span>Error: {error}</span>
      </div>
    )
  }

  if (!resultSet) {
    return (
      <div className={styles.message}>
        <span>Execute a query to see results</span>
      </div>
    )
  }

  return (
    <div className={styles.container}>
      <div className={`ag-theme-alpine-dark ${styles.grid}`}>
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
          suppressRowClickSelection={true}
          suppressCellFocus={false}
          enableRangeSelection={false}
          copyHeadersToClipboard={true}
          suppressCopyRowsToClipboard={false}
        />
      </div>
      <div className={styles.statusBar}>
        <span>{resultSet.rows.length} rows</span>
        <span>|</span>
        <span>{resultSet.executionTimeMs.toFixed(2)} ms</span>
        {resultSet.affectedRows > 0 && (
          <>
            <span>|</span>
            <span>{resultSet.affectedRows} affected</span>
          </>
        )}
      </div>
    </div>
  )
}
