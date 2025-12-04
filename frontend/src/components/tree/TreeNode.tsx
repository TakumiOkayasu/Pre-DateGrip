import { memo } from 'react';
import type { DatabaseObject } from '../../types';
import styles from './TreeNode.module.css';

interface TreeNodeProps {
  node: DatabaseObject;
  level: number;
  expandedNodes: Set<string>;
  loadingNodes?: Set<string>;
  selectedNodeId?: string | null;
  onToggle: (id: string, node: DatabaseObject) => void;
  onTableOpen?: (nodeId: string, tableName: string, tableType: 'table' | 'view') => void;
}

const getIcon = (type: DatabaseObject['type'] | 'folder'): string => {
  switch (type) {
    case 'database':
      return '🗄️';
    case 'folder':
      return '📁';
    case 'table':
      return '🗃️';
    case 'view':
      return '👁️';
    case 'procedure':
      return '⚙️';
    case 'function':
      return 'ƒ';
    case 'column':
      return '│';
    case 'index':
      return '🔑';
    default:
      return '📄';
  }
};

export const TreeNode = memo(function TreeNode({
  node,
  level,
  expandedNodes,
  loadingNodes,
  selectedNodeId,
  onToggle,
  onTableOpen,
}: TreeNodeProps) {
  const hasChildren = node.children && node.children.length > 0;
  const canExpand = hasChildren || node.type === 'table'; // Tables can lazy-load columns
  const isExpanded = expandedNodes.has(node.id);
  const isLoading = loadingNodes?.has(node.id);
  const isSelected = selectedNodeId === node.id;

  const handleClick = () => {
    // For tables/views, single click opens data; arrow click expands columns
    if ((node.type === 'table' || node.type === 'view') && onTableOpen) {
      onTableOpen(node.id, node.name, node.type);
    } else if (canExpand) {
      onToggle(node.id, node);
    }
  };

  const handleExpanderClick = (e: React.MouseEvent) => {
    e.stopPropagation();
    if (canExpand) {
      onToggle(node.id, node);
    }
  };

  const handleContextMenu = (e: React.MouseEvent) => {
    e.preventDefault();
    // TODO: Show context menu
    console.log('Context menu:', node);
  };

  const getExpander = () => {
    if (isLoading) return '⏳';
    if (canExpand) return isExpanded ? '▼' : '▶';
    return ' ';
  };

  const nodeClasses = [
    styles.node,
    isLoading ? styles.loading : '',
    isSelected ? styles.selected : '',
  ]
    .filter(Boolean)
    .join(' ');

  return (
    <div className={styles.container}>
      <div
        className={nodeClasses}
        style={{ paddingLeft: `${level * 16 + 8}px` }}
        onClick={handleClick}
        onContextMenu={handleContextMenu}
      >
        <span className={styles.expander} onClick={handleExpanderClick} role="button" tabIndex={-1}>
          {getExpander()}
        </span>
        <span className={styles.icon}>{getIcon(node.type)}</span>
        <span className={styles.name}>{node.name}</span>
      </div>

      {hasChildren && isExpanded && (
        <div className={styles.children}>
          {node.children?.map((child) => (
            <TreeNode
              key={child.id}
              node={child}
              level={level + 1}
              expandedNodes={expandedNodes}
              loadingNodes={loadingNodes}
              selectedNodeId={selectedNodeId}
              onToggle={onToggle}
              onTableOpen={onTableOpen}
            />
          ))}
        </div>
      )}
    </div>
  );
});
