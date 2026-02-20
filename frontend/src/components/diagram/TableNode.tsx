import { Handle, type HandleType, Position } from '@xyflow/react';
import { memo } from 'react';
import type { ERColumn } from '../../types';
import { readableColor } from '../../utils/colorContrast';
import styles from './TableNode.module.css';

interface TableNodeData {
  tableName: string;
  logicalName?: string;
  columns: ERColumn[];
  color?: string;
  bkColor?: string;
}

interface TableNodeProps {
  data: TableNodeData;
  selected?: boolean;
}

const icons = {
  table: '\uD83D\uDCCB', // 📋
  key: '\uD83D\uDD11', // 🔑
};

const INT_RE = /\b(tiny|small|big)?int\b/i;
const HIDDEN_HANDLE: React.CSSProperties = { opacity: 0, width: 6, height: 6 };

type HandleDef = { type: HandleType; position: Position; id: string };
const HANDLE_DEFS: HandleDef[] = [
  { type: 'target', position: Position.Top, id: 'target-top' },
  { type: 'target', position: Position.Right, id: 'target-right' },
  { type: 'target', position: Position.Bottom, id: 'target-bottom' },
  { type: 'target', position: Position.Left, id: 'target-left' },
  { type: 'source', position: Position.Top, id: 'source-top' },
  { type: 'source', position: Position.Right, id: 'source-right' },
  { type: 'source', position: Position.Bottom, id: 'source-bottom' },
  { type: 'source', position: Position.Left, id: 'source-left' },
];

function ColumnRow({ col, isPK }: { col: ERColumn; isPK: boolean }) {
  const isInt = col.type ? INT_RE.test(col.type) : false;
  const showLogical = col.logicalName && col.logicalName !== col.name;

  return (
    <div
      className={`${styles.column} ${isPK ? styles.primaryKey : ''}`}
      title={showLogical ? col.name : col.comment || undefined}
    >
      {isPK && <span className={styles.keyIcon}>{icons.key}</span>}
      <span className={styles.columnName}>
        {!isPK && !col.nullable ? '*' : ''}
        {showLogical ? col.logicalName : col.name}
        {showLogical && <span className={styles.logicalName}>({col.name})</span>}
      </span>
      {col.type && (
        <span className={`${styles.columnType} ${isInt ? styles.intType : ''}`}>
          {col.type.toLowerCase()}
        </span>
      )}
      {col.color && <span className={styles.colorDot} style={{ backgroundColor: col.color }} />}
    </div>
  );
}

export const TableNode = memo(function TableNode({ data, selected }: TableNodeProps) {
  const { tableName, logicalName, columns, color, bkColor } = data;

  const primaryKeys = columns.filter((c) => c.isPrimaryKey);
  const regularColumns = columns.filter((c) => !c.isPrimaryKey);

  return (
    <div className={`${styles.container} ${selected ? styles.selected : ''}`}>
      {HANDLE_DEFS.map((h) => (
        <Handle key={h.id} type={h.type} position={h.position} id={h.id} style={HIDDEN_HANDLE} />
      ))}

      <div
        className={styles.header}
        title={logicalName ? tableName : undefined}
        style={
          bkColor ? { backgroundColor: bkColor, color: readableColor(bkColor, color) } : undefined
        }
      >
        <span className={styles.icon}>{icons.table}</span>
        <span className={styles.tableName}>{logicalName || tableName}</span>
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
    </div>
  );
});
