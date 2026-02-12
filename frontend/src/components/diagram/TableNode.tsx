import { Handle, Position } from '@xyflow/react';
import { memo } from 'react';
import type { ERColumn } from '../../types';
import styles from './TableNode.module.css';

interface TableNodeData {
  tableName: string;
  columns: ERColumn[];
}

interface TableNodeProps {
  data: TableNodeData;
  selected?: boolean;
}

// Unicode icons
const icons = {
  table: '\uD83D\uDCCB', // 📋
  key: '\uD83D\uDD11', // 🔑
};

function ColumnRow({ col, isPK }: { col: ERColumn; isPK: boolean }) {
  return (
    <div
      className={`${styles.column} ${isPK ? styles.primaryKey : ''}`}
      title={col.comment || undefined}
    >
      {isPK && <span className={styles.keyIcon}>{icons.key}</span>}
      <span className={styles.columnName}>
        {!isPK && !col.nullable ? '*' : ''}
        {col.name}
        {col.logicalName && <span className={styles.logicalName}> ({col.logicalName})</span>}
      </span>
      <span className={styles.columnType}>{col.type}</span>
      {col.defaultValue && <span className={styles.defaultValue}>={col.defaultValue}</span>}
    </div>
  );
}

export const TableNode = memo(function TableNode({ data, selected }: TableNodeProps) {
  const { tableName, columns } = data;

  const primaryKeys = columns.filter((c) => c.isPrimaryKey);
  const regularColumns = columns.filter((c) => !c.isPrimaryKey);

  return (
    <div className={`${styles.container} ${selected ? styles.selected : ''}`}>
      <Handle type="target" position={Position.Left} className={styles.handle} />

      <div className={styles.header}>
        <span className={styles.icon}>{icons.table}</span>
        <span className={styles.tableName}>{tableName}</span>
      </div>

      <div className={styles.columns}>
        {primaryKeys.map((col, i) => (
          <ColumnRow key={`pk-${i}-${col.name}`} col={col} isPK />
        ))}

        {primaryKeys.length > 0 && regularColumns.length > 0 && <div className={styles.divider} />}

        {regularColumns.map((col, i) => (
          <ColumnRow key={`col-${i}-${col.name}`} col={col} isPK={false} />
        ))}
      </div>

      <Handle type="source" position={Position.Right} className={styles.handle} />
    </div>
  );
});
