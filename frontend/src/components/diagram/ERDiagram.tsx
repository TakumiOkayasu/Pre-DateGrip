import {
  Background,
  BackgroundVariant,
  Controls,
  type Edge,
  MarkerType,
  type Node,
  ReactFlow,
  useEdgesState,
  useNodesState,
} from '@xyflow/react';
import { useCallback, useMemo } from 'react';
import '@xyflow/react/dist/style.css';
import { useERDiagramContext } from '../../hooks/useERDiagramContext';
import type { ERRelationEdge, ERTableNode } from '../../types';
import { ALL_PAGES, GRID_LAYOUT } from '../../utils/erDiagramConstants';
import styles from './ERDiagram.module.css';
import { TableNode } from './TableNode';

interface ERDiagramProps {
  onTableClick?: (tableId: string) => void;
}

const nodeTypes = {
  table: TableNode,
};

/** 「すべて」タブ時のグリッド再配置 */
function applyGridLayout(tables: ERTableNode[]): ERTableNode[] {
  const { columns, nodeWidth, nodeHeight, gap } = GRID_LAYOUT;
  return tables.map((t, i) => ({
    ...t,
    position: {
      x: (i % columns) * (nodeWidth + gap),
      y: Math.floor(i / columns) * (nodeHeight + gap),
    },
  }));
}

function ERDiagramFlow({
  tables,
  relations,
  onTableClick,
}: {
  tables: ERTableNode[];
  relations: ERRelationEdge[];
  onTableClick?: (tableId: string) => void;
}) {
  const initialNodes: Node[] = useMemo(
    () =>
      tables.map((table) => ({
        id: table.id,
        type: 'table',
        position: table.position,
        data: table.data,
      })),
    [tables]
  );

  const initialEdges: Edge[] = useMemo(
    () =>
      relations.map((rel) => ({
        id: rel.id,
        source: rel.source,
        target: rel.target,
        type: 'smoothstep',
        animated: false,
        label: rel.data.cardinality,
        labelStyle: { fontSize: 10, fill: '#888' },
        labelBgStyle: { fill: '#1e1e1e', fillOpacity: 0.8 },
        markerEnd: {
          type: MarkerType.ArrowClosed,
          width: 15,
          height: 15,
          color: '#888',
        },
        style: { stroke: '#888', strokeWidth: 1 },
      })),
    [relations]
  );

  const [nodes, , onNodesChange] = useNodesState(initialNodes);
  const [edges, , onEdgesChange] = useEdgesState(initialEdges);

  const handleNodeClick = useCallback(
    (_: React.MouseEvent, node: Node) => {
      onTableClick?.(node.id);
    },
    [onTableClick]
  );

  return (
    <ReactFlow
      nodes={nodes}
      edges={edges}
      onNodesChange={onNodesChange}
      onEdgesChange={onEdgesChange}
      onNodeClick={handleNodeClick}
      nodeTypes={nodeTypes}
      fitView
      minZoom={0.1}
      maxZoom={2}
      defaultViewport={{ x: 0, y: 0, zoom: 0.8 }}
    >
      <Background variant={BackgroundVariant.Dots} gap={20} size={1} color="#333" />
      <Controls />
    </ReactFlow>
  );
}

export function ERDiagram({ onTableClick }: ERDiagramProps) {
  const {
    pages,
    selectedPage,
    setSelectedPage,
    pageCounts,
    tables: filteredTables,
    relations: filteredRelations,
  } = useERDiagramContext();

  // 「すべて」タブ時はグリッド再配置（ページ間で座標が重複するため）
  const layoutTables = useMemo(
    () => (selectedPage === ALL_PAGES ? applyGridLayout(filteredTables) : filteredTables),
    [filteredTables, selectedPage]
  );

  const showTabs = pages.length > 1;

  // key を変えることで ReactFlow を再マウントし、fitView をリトリガー
  const flowKey = useMemo(() => {
    const first = layoutTables[0]?.id ?? '';
    const last = layoutTables[layoutTables.length - 1]?.id ?? '';
    return `${selectedPage}-${layoutTables.length}-${first}-${last}`;
  }, [selectedPage, layoutTables]);

  return (
    <div className={styles.container}>
      {showTabs && (
        <div className={styles.tabBar}>
          <button
            type="button"
            className={`${styles.tab} ${selectedPage === ALL_PAGES ? styles.tabActive : ''}`}
            onClick={() => setSelectedPage(ALL_PAGES)}
          >
            すべて ({pageCounts.get(ALL_PAGES) ?? 0})
          </button>
          {pages.map((page) => (
            <button
              type="button"
              key={page}
              className={`${styles.tab} ${selectedPage === page ? styles.tabActive : ''}`}
              onClick={() => setSelectedPage(page)}
            >
              {page} ({pageCounts.get(page) ?? 0})
            </button>
          ))}
        </div>
      )}
      <div className={styles.flowContainer}>
        <ERDiagramFlow
          key={flowKey}
          tables={layoutTables}
          relations={filteredRelations}
          onTableClick={onTableClick}
        />
      </div>
    </div>
  );
}
