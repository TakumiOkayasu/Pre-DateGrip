import { create } from 'zustand';
import { bridge, toERDiagramModel } from '../api/bridge';
import type { ERRelationEdge, ERTableNode } from '../types';
import { DEFAULT_PAGE, GRID_LAYOUT } from '../utils/erDiagramConstants';
import type { ERDiagramModel } from '../utils/erDiagramParser';
import { extractPages } from '../utils/erDiagramUtils';

interface ERDiagramState {
  tables: ERTableNode[];
  relations: ERRelationEdge[];
  isLoading: boolean;
  error: string | null;

  // Page state
  selectedPage: string;

  // Actions
  setTables: (tables: ERTableNode[]) => void;
  setRelations: (relations: ERRelationEdge[]) => void;
  addTable: (table: ERTableNode) => void;
  updateTablePosition: (id: string, position: { x: number; y: number }) => void;
  removeTable: (id: string) => void;
  clearDiagram: () => void;
  setSelectedPage: (page: string) => void;

  // Reverse engineering
  loadFromDatabase: (connectionId: string, database: string) => Promise<void>;

  // Import from A5:ER file
  loadFromA5ERFile: (filepath: string) => Promise<void>;

  // Import from parsed ER diagram model
  loadFromParsedModel: (model: ERDiagramModel) => void;
}

export const useERDiagramStore = create<ERDiagramState>((set, get) => ({
  tables: [],
  relations: [],
  isLoading: false,
  error: null,
  selectedPage: DEFAULT_PAGE,

  setTables: (tables) => set({ tables }),

  setRelations: (relations) => set({ relations }),

  addTable: (table) => {
    set((state) => ({
      tables: [...state.tables, table],
    }));
  },

  updateTablePosition: (id, position) => {
    set((state) => ({
      tables: state.tables.map((t) => (t.id === id ? { ...t, position } : t)),
    }));
  },

  removeTable: (id) => {
    set((state) => ({
      tables: state.tables.filter((t) => t.id !== id),
      relations: state.relations.filter((r) => r.source !== id && r.target !== id),
    }));
  },

  clearDiagram: () => {
    set({ tables: [], relations: [], selectedPage: DEFAULT_PAGE });
  },

  setSelectedPage: (page) => set({ selectedPage: page }),

  loadFromA5ERFile: async (filepath) => {
    set({ isLoading: true, error: null });

    try {
      const result = await bridge.parseA5ER(filepath);
      const model = toERDiagramModel(result);
      get().loadFromParsedModel(model);
      set({ isLoading: false });
    } catch (err) {
      set({
        isLoading: false,
        error: err instanceof Error ? err.message : 'Failed to load A5:ER file',
      });
    }
  },

  loadFromDatabase: async (connectionId, database) => {
    set({ isLoading: true, error: null });

    try {
      // Get tables
      const { tables: tablesData } = await bridge.getTables(connectionId, database);
      const tables: ERTableNode[] = [];
      const relations: ERRelationEdge[] = [];

      const { columns: gridColumns, nodeWidth, nodeHeight, gap } = GRID_LAYOUT;

      // Load columns for each table
      for (let i = 0; i < tablesData.length; i++) {
        const tableInfo = tablesData[i];
        if (tableInfo.type !== 'TABLE' && tableInfo.type !== 'VIEW') continue;

        const fullTableName = tableInfo.schema
          ? `${tableInfo.schema}.${tableInfo.name}`
          : tableInfo.name;

        try {
          const columnsData = await bridge.getColumns(connectionId, fullTableName);

          const col = i % gridColumns;
          const row = Math.floor(i / gridColumns);

          tables.push({
            id: fullTableName,
            type: 'table',
            data: {
              tableName: tableInfo.name,
              columns: columnsData.map((c) => ({
                name: c.name,
                type: c.type,
                size: c.size,
                nullable: c.nullable,
                isPrimaryKey: c.isPrimaryKey,
              })),
            },
            position: {
              x: col * (nodeWidth + gap),
              y: row * (nodeHeight + gap),
            },
          });
        } catch (err) {
          console.warn(`Failed to load columns for ${fullTableName}:`, err);
        }
      }

      // Try to detect foreign key relationships
      // This is a simplified detection based on naming convention
      tables.forEach((sourceTable) => {
        sourceTable.data.columns.forEach((col) => {
          // Check for columns ending with _id
          if (col.name.toLowerCase().endsWith('_id') && !col.isPrimaryKey) {
            const potentialTableName = col.name.slice(0, -3); // Remove _id

            // Find target table
            const targetTable = tables.find(
              (t) =>
                t.data.tableName.toLowerCase() === potentialTableName.toLowerCase() ||
                t.data.tableName.toLowerCase() === `${potentialTableName.toLowerCase()}s`
            );

            if (targetTable) {
              relations.push({
                id: `${sourceTable.id}-${targetTable.id}-${col.name}`,
                source: targetTable.id,
                target: sourceTable.id,
                type: 'relation',
                data: {
                  cardinality: '1:N',
                  sourceColumn: 'id',
                  targetColumn: col.name,
                },
              });
            }
          }
        });
      });

      set({ tables, relations, isLoading: false, selectedPage: DEFAULT_PAGE });
    } catch (err) {
      set({
        isLoading: false,
        error: err instanceof Error ? err.message : 'Failed to load database schema',
      });
    }
  },

  loadFromParsedModel: (model) => {
    const { columns: gridColumns, nodeWidth, nodeHeight, gap } = GRID_LAYOUT;

    const erTables: ERTableNode[] = model.tables.map((table, i) => {
      // ファイルに位置情報があればそれを使用、なければ自動レイアウト
      const hasPosition = table.posX !== 0 || table.posY !== 0;
      const col = i % gridColumns;
      const row = Math.floor(i / gridColumns);

      return {
        id: table.name,
        type: 'table',
        data: {
          tableName: table.name,
          logicalName: table.logicalName || undefined,
          page: table.page || DEFAULT_PAGE,
          columns: table.columns.map((c) => ({
            name: c.name,
            type: c.type || undefined,
            size: 0,
            nullable: c.nullable,
            isPrimaryKey: c.isPrimaryKey,
            logicalName: c.logicalName || undefined,
            defaultValue: c.defaultValue || undefined,
            comment: c.logicalName || c.comment || undefined,
          })),
        },
        position: hasPosition
          ? { x: table.posX, y: table.posY }
          : { x: col * (nodeWidth + gap), y: row * (nodeHeight + gap) },
      };
    });

    const erRelations: ERRelationEdge[] = model.relations.map((rel, i) => ({
      id: `rel-${i}-${rel.name}`,
      source: rel.sourceTable,
      target: rel.targetTable,
      type: 'relation',
      data: {
        cardinality: rel.cardinality,
        sourceColumn: rel.sourceColumn,
        targetColumn: rel.targetColumn,
      },
    }));

    // 最初のページを初期選択
    const pages = extractPages(erTables);
    set({
      tables: erTables,
      relations: erRelations,
      selectedPage: pages[0] || DEFAULT_PAGE,
    });
  },
}));
