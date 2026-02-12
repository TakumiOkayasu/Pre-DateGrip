import { memo } from 'react';
import styles from './ResultGrid.module.css';

interface GridFilterBarProps {
  whereClause: string;
  isExecuting: boolean;
  onWhereChange: (value: string) => void;
  onApply: () => void;
  onClear: () => void;
  onKeyDown: (e: React.KeyboardEvent<HTMLInputElement>) => void;
}

function GridFilterBarInner({
  whereClause,
  isExecuting,
  onWhereChange,
  onApply,
  onClear,
  onKeyDown,
}: GridFilterBarProps) {
  return (
    <div className={styles.filterBar}>
      <span className={styles.filterLabel}>WHERE</span>
      <input
        type="text"
        className={styles.filterInput}
        placeholder="例: id > 100 AND name LIKE '%test%'"
        value={whereClause}
        onChange={(e) => onWhereChange(e.target.value)}
        onKeyDown={onKeyDown}
      />
      <button
        type="button"
        onClick={onApply}
        className={styles.toolbarButton}
        disabled={isExecuting}
        title="WHEREフィルタを適用 (Enter)"
      >
        適用
      </button>
      <button
        type="button"
        onClick={onClear}
        className={styles.toolbarButton}
        disabled={isExecuting || !whereClause}
        title="フィルタをクリア"
      >
        クリア
      </button>
    </div>
  );
}

export const GridFilterBar = memo(GridFilterBarInner);
