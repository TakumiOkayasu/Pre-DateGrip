import {
  type ColumnDef,
  type ColumnFiltersState,
  getCoreRowModel,
  getFilteredRowModel,
  getSortedRowModel,
  type SortingState,
  useReactTable,
} from '@tanstack/react-table';
import { useVirtualizer } from '@tanstack/react-virtual';
import { memo, useCallback, useMemo, useRef, useState } from 'react';
import { useConnectionStore } from '../../store/connectionStore';
import {
  useIsActiveDataView,
  useIsQueryExecuting,
  useQueryActions,
  useQueryById,
  useQueryError,
  useQueryResult,
  useQueryStore,
} from '../../store/queryStore';
import { useSessionStore } from '../../store/sessionStore';
import type { ResultSet } from '../../types';
import { isNumericType, type RowData } from '../../types/grid';
import { log } from '../../utils/logger';
import { ExportDialog } from '../export/ExportDialog';
import { GridFilterBar } from './GridFilterBar';
import { GridStatusBar } from './GridStatusBar';
import { GridTable } from './GridTable';
import { GridToolbar } from './GridToolbar';
import { useColumnAutoSize } from './hooks/useColumnAutoSize';
import { useGridEdit } from './hooks/useGridEdit';
import { useGridKeyboard } from './hooks/useGridKeyboard';
import { useRelatedRows } from './hooks/useRelatedRows';
import styles from './ResultGrid.module.css';
import { ResultTabs } from './ResultTabs';
import { ValueEditorDialog } from './ValueEditorDialog';

interface ResultGridProps {
  queryId?: string;
  excludeDataView?: boolean;
}

function ResultGridInner({ queryId, excludeDataView = false }: ResultGridProps = {}) {
  // --- Store subscriptions ---
  const activeQueryId = useQueryStore((state) => state.activeQueryId);
  const activeConnectionId = useConnectionStore((state) => state.activeConnectionId);
  const isActiveDataView = useIsActiveDataView();
  const targetQueryId = excludeDataView && isActiveDataView ? null : (queryId ?? activeQueryId);
  const currentQuery = useQueryById(targetQueryId);
  const queryResult = useQueryResult(targetQueryId);
  const isExecuting = useIsQueryExecuting(targetQueryId);
  const error = useQueryError(targetQueryId);
  const showLogicalNamesInGrid = useSessionStore((state) => state.showLogicalNamesInGrid);
  const setShowLogicalNamesInGrid = useSessionStore((state) => state.setShowLogicalNamesInGrid);
  const { applyWhereFilter, refreshDataView, openTableData } = useQueryActions();

  // --- Local state ---
  const [whereClause, setWhereClause] = useState('');
  const [isExportDialogOpen, setIsExportDialogOpen] = useState(false);
  const [selectedRows, setSelectedRows] = useState<Set<number>>(new Set());
  const [selectedColumn, setSelectedColumn] = useState<string | null>(null);
  const [activeResultIndex, setActiveResultIndex] = useState(0);
  const [valueEditorState, setValueEditorState] = useState<{
    isOpen: boolean;
    rowIndex: number;
    columnName: string;
    value: string | null;
  }>({ isOpen: false, rowIndex: 0, columnName: '', value: null });
  const [sorting, setSorting] = useState<SortingState>([]);
  const [columnFilters, setColumnFilters] = useState<ColumnFiltersState>([]);
  const [showColumnFilters, setShowColumnFilters] = useState(false);
  const tableContainerRef = useRef<HTMLDivElement>(null);

  // --- Derived data ---
  const multipleResult = queryResult && 'multipleResults' in queryResult ? queryResult : null;
  const filteredResults = multipleResult
    ? multipleResult.results.filter((r) => !r.statement.trim().toUpperCase().startsWith('USE '))
    : null;
  const hasFilteredResults = filteredResults && filteredResults.length > 0;
  const resultSet: ResultSet | null = hasFilteredResults
    ? (filteredResults[activeResultIndex]?.data ?? null)
    : multipleResult
      ? null
      : (queryResult as ResultSet | null);

  // --- Row / Column data ---
  const baseRowData = useMemo<RowData[]>(() => {
    if (!resultSet) return [];
    const rows = resultSet.rows;
    const cols = resultSet.columns;
    const result = new Array(rows.length);
    for (let rowIndex = 0; rowIndex < rows.length; rowIndex++) {
      const row = rows[rowIndex];
      const obj: Record<string, string | null> = {
        __rowIndex: String(rowIndex + 1),
        __originalIndex: String(rowIndex),
      };
      for (let colIdx = 0; colIdx < cols.length; colIdx++) {
        const value = row[colIdx];
        obj[cols[colIdx].name] = value === '' || value === undefined ? null : value;
      }
      result[rowIndex] = obj;
    }
    return result;
  }, [resultSet]);

  const columns = useMemo<ColumnDef<RowData>[]>(() => {
    if (!resultSet) return [];
    return resultSet.columns.map((col) => {
      const isNumeric = isNumericType(col.type);
      const displayName = showLogicalNamesInGrid && col.comment ? col.comment : col.name;
      return {
        id: col.name,
        header: displayName,
        accessorKey: col.name,
        size: 150,
        minSize: 80,
        meta: { type: col.type, align: isNumeric ? 'right' : 'left' },
      };
    });
  }, [resultSet, showLogicalNamesInGrid]);

  // --- Hooks ---
  const { columnSizing, setColumnSizing } = useColumnAutoSize({
    resultSet,
    columns,
    rowData: baseRowData,
  });

  const {
    isEditMode,
    hasChanges,
    isApplying,
    applyError,
    isRowDeleted,
    isRowInserted,
    getInsertedRows,
    getCellChange,
    updateCell,
    handleToggleEditMode,
    handleRevertChanges,
    handleDeleteRow,
    handleCloneRow,
    handleApplyChanges,
  } = useGridEdit({
    resultSet,
    currentQuery,
    activeConnectionId,
    rowData: baseRowData,
    selectedRows,
  });

  const rowData = useMemo<RowData[]>(() => {
    const insertedRows = getInsertedRows();
    if (insertedRows.size === 0) return baseRowData;
    const combined = [...baseRowData];
    insertedRows.forEach((rowValues, rowIndex) => {
      combined.push({ ...rowValues, __rowIndex: '新規', __originalIndex: String(rowIndex) });
    });
    return combined;
  }, [baseRowData, getInsertedRows]);

  const handleOpenRelatedTable = useCallback(
    (tableName: string, fkWhereClause: string) => {
      if (activeConnectionId) openTableData(activeConnectionId, tableName, fkWhereClause);
    },
    [activeConnectionId, openTableData]
  );

  const { isForeignKeyColumn, navigateToRelatedRow } = useRelatedRows({
    connectionId: activeConnectionId,
    tableName: currentQuery?.sourceTable ?? null,
    onOpenRelatedTable: handleOpenRelatedTable,
  });

  const handleNavigateRelated = useCallback(
    (rowIndex: number, columnName: string) => {
      const row = rowData[rowIndex];
      if (row) navigateToRelatedRow(columnName, row);
    },
    [rowData, navigateToRelatedRow]
  );

  const handleOpenValueEditor = useCallback(
    (rowIndex: number, columnName: string, currentValue: string | null) => {
      setValueEditorState({ isOpen: true, rowIndex, columnName, value: currentValue });
    },
    []
  );

  const handleValueEditorSave = useCallback(
    (newValue: string | null) => {
      const { rowIndex, columnName, value: oldValue } = valueEditorState;
      if (oldValue !== newValue) updateCell(rowIndex, columnName, oldValue, newValue);
      setValueEditorState((prev) => ({ ...prev, isOpen: false }));
    },
    [valueEditorState, updateCell]
  );

  const { editingCell, editValue, setEditValue, handleStartEdit, handleConfirmEdit } =
    useGridKeyboard({
      isEditMode,
      selectedRows,
      selectedColumn,
      columns,
      rowData,
      tableContainerRef,
      updateCell,
      onDeleteRow: handleDeleteRow,
      onCloneRow: handleCloneRow,
      onNavigateRelated: handleNavigateRelated,
      onOpenValueEditor: handleOpenValueEditor,
    });

  const table = useReactTable({
    data: rowData,
    columns,
    getCoreRowModel: getCoreRowModel(),
    getFilteredRowModel: getFilteredRowModel(),
    getSortedRowModel: getSortedRowModel(),
    enableRowSelection: true,
    enableColumnResizing: true,
    enableSorting: true,
    enableColumnFilters: true,
    columnResizeMode: 'onChange',
    state: { columnSizing, sorting, columnFilters },
    onColumnSizingChange: setColumnSizing,
    onSortingChange: setSorting,
    onColumnFiltersChange: setColumnFilters,
  });

  const { rows } = table.getRowModel();
  const rowVirtualizer = useVirtualizer({
    count: rows.length,
    getScrollElement: () => tableContainerRef.current,
    estimateSize: () => 32,
    overscan: 10,
  });
  const virtualRows = rowVirtualizer.getVirtualItems();
  const virtualTotalSize = rowVirtualizer.getTotalSize();

  // --- Stable props for GridTable (avoid memo breakage) ---
  const gridEditState = useMemo(
    () => ({
      isEditMode,
      editingCell,
      editValue,
      isRowDeleted,
      isRowInserted,
      getCellChange,
      isForeignKeyColumn,
    }),
    [
      isEditMode,
      editingCell,
      editValue,
      isRowDeleted,
      isRowInserted,
      getCellChange,
      isForeignKeyColumn,
    ]
  );

  const gridSelectionState = useMemo(
    () => ({ selectedRows, selectedColumn }),
    [selectedRows, selectedColumn]
  );

  // --- Callbacks for sub-components ---
  const handleRowClick = useCallback((rowIndex: number) => {
    setSelectedRows((prev) => {
      const next = new Set(prev);
      if (next.has(rowIndex)) next.delete(rowIndex);
      else next.add(rowIndex);
      return next;
    });
  }, []);

  const handleCellClick = useCallback((rowIndex: number, field: string) => {
    setSelectedRows(new Set([rowIndex]));
    setSelectedColumn(field);
  }, []);

  const gridCallbacks = useMemo(
    () => ({
      onSetEditValue: setEditValue,
      onStartEdit: handleStartEdit,
      onConfirmEdit: handleConfirmEdit,
      onRowClick: handleRowClick,
      onCellClick: handleCellClick,
    }),
    [setEditValue, handleStartEdit, handleConfirmEdit, handleRowClick, handleCellClick]
  );

  const handleWhereKeyDown = useCallback(
    (e: React.KeyboardEvent<HTMLInputElement>) => {
      if (e.key === 'Enter' && activeQueryId && activeConnectionId && currentQuery?.sourceTable) {
        applyWhereFilter(activeQueryId, activeConnectionId, whereClause);
      }
    },
    [activeQueryId, activeConnectionId, currentQuery?.sourceTable, whereClause, applyWhereFilter]
  );

  const handleWhereApply = useCallback(() => {
    if (activeQueryId && activeConnectionId) {
      applyWhereFilter(activeQueryId, activeConnectionId, whereClause);
    }
  }, [activeQueryId, activeConnectionId, whereClause, applyWhereFilter]);

  const handleWhereClear = useCallback(() => {
    setWhereClause('');
    if (activeQueryId && activeConnectionId) {
      applyWhereFilter(activeQueryId, activeConnectionId, '');
    }
  }, [activeQueryId, activeConnectionId, applyWhereFilter]);

  const handleRefresh = useCallback(() => {
    if (targetQueryId && activeConnectionId) {
      refreshDataView(targetQueryId, activeConnectionId);
    }
  }, [targetQueryId, activeConnectionId, refreshDataView]);

  const handleToggleColumnFilters = useCallback(() => {
    setShowColumnFilters((prev) => !prev);
    if (showColumnFilters) setColumnFilters([]);
  }, [showColumnFilters]);

  const handleExport = useCallback(() => setIsExportDialogOpen(true), []);

  // --- Early returns ---
  if (isExecuting) {
    return (
      <div className={styles.message}>
        <span className={styles.spinner}>{'\u23F3'}</span>
        <span>クエリ実行中...</span>
      </div>
    );
  }

  if (error) {
    log.debug(`[ResultGrid] Showing error: ${error}`);
    return (
      <div className={`${styles.message} ${styles.error}`}>
        <span>エラー: {error}</span>
      </div>
    );
  }

  if (!resultSet) {
    return (
      <div className={styles.message}>
        <span>クエリを実行して結果を表示</span>
      </div>
    );
  }

  // --- Render ---
  return (
    <div className={styles.container}>
      {hasFilteredResults && (
        <ResultTabs
          results={filteredResults}
          activeIndex={activeResultIndex}
          onSelect={setActiveResultIndex}
        />
      )}

      <GridToolbar
        showRefresh={!!currentQuery?.sourceTable && !!activeConnectionId}
        isEditMode={isEditMode}
        hasChanges={hasChanges()}
        isApplying={isApplying}
        applyError={applyError}
        showLogicalNamesInGrid={showLogicalNamesInGrid}
        showColumnFilters={showColumnFilters}
        onRefresh={handleRefresh}
        onToggleEditMode={handleToggleEditMode}
        onDeleteRow={handleDeleteRow}
        onRevertChanges={handleRevertChanges}
        onApplyChanges={handleApplyChanges}
        onSetShowLogicalNames={setShowLogicalNamesInGrid}
        onToggleColumnFilters={handleToggleColumnFilters}
        onExport={handleExport}
      />

      {currentQuery?.sourceTable && (
        <GridFilterBar
          whereClause={whereClause}
          isExecuting={isExecuting}
          onWhereChange={setWhereClause}
          onApply={handleWhereApply}
          onClear={handleWhereClear}
          onKeyDown={handleWhereKeyDown}
        />
      )}

      <GridTable
        table={table}
        tableContainerRef={tableContainerRef}
        rows={rows}
        virtualRows={virtualRows}
        totalSize={virtualTotalSize}
        showColumnFilters={showColumnFilters}
        showLogicalNamesInGrid={showLogicalNamesInGrid}
        edit={gridEditState}
        selection={gridSelectionState}
        callbacks={gridCallbacks}
      />

      <div className={styles.statusBar}>
        <span>
          {columnFilters.length > 0
            ? resultSet.truncated
              ? `${rows.length} / ${resultSet.rows.length.toLocaleString()}+ 件 (フィルタ中・行数制限あり)`
              : `${rows.length} / ${resultSet.rows.length} 件 (フィルタ中)`
            : resultSet.truncated
              ? `先頭 ${resultSet.rows.length.toLocaleString()} 件を表示（テーブルにはさらにデータがあります）`
              : `${resultSet.rows.length} 件`}
        </span>
        <span>|</span>
        <span>{resultSet.executionTimeMs.toFixed(2)} ms</span>
        {resultSet.affectedRows > 0 && (
          <>
            <span>|</span>
            <span>{resultSet.affectedRows} 件更新</span>
          </>
        )}
        {isEditMode && <span className={styles.editModeIndicator}>編集モード</span>}
      </div>

      <ExportDialog
        isOpen={isExportDialogOpen}
        onClose={() => setIsExportDialogOpen(false)}
        resultSet={resultSet}
      />

      <ValueEditorDialog
        isOpen={valueEditorState.isOpen}
        columnName={valueEditorState.columnName}
        initialValue={valueEditorState.value}
        onSave={handleValueEditorSave}
        onCancel={() => setValueEditorState((prev) => ({ ...prev, isOpen: false }))}
      />
    </div>
  );
}

export const ResultGrid = memo(ResultGridInner);
