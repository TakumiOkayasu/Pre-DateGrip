import type { IPCRequest, IPCResponse } from '../types';

declare global {
  interface Window {
    invoke?: (request: string) => Promise<string>;
  }
}

// Development mode mock data
const mockData: Record<string, unknown> = {
  connect: { connectionId: 'mock-conn-1' },
  disconnect: undefined,
  testConnection: { success: true, message: 'Mock connection successful' },
  executeQuery: {
    columns: [
      { name: 'id', type: 'int' },
      { name: 'name', type: 'nvarchar' },
      { name: 'created_at', type: 'datetime' },
    ],
    rows: [
      ['1', 'Test Item 1', '2024-01-01 00:00:00'],
      ['2', 'Test Item 2', '2024-01-02 00:00:00'],
      ['3', 'Test Item 3', '2024-01-03 00:00:00'],
    ],
    affectedRows: 0,
    executionTimeMs: 15,
    cached: false,
  },
  getCacheStats: {
    currentSizeBytes: 0,
    maxSizeBytes: 104857600,
    usagePercent: 0,
  },
  clearCache: { cleared: true },
  executeAsyncQuery: { queryId: 'mock-query-1' },
  getAsyncQueryResult: {
    queryId: 'mock-query-1',
    status: 'completed',
    columns: [
      { name: 'id', type: 'int' },
      { name: 'name', type: 'nvarchar' },
    ],
    rows: [
      ['1', 'Test Item 1'],
      ['2', 'Test Item 2'],
    ],
    affectedRows: 0,
    executionTimeMs: 50,
  },
  cancelAsyncQuery: { cancelled: true },
  getActiveQueries: [],
  filterResultSet: {
    columns: [
      { name: 'id', type: 'int' },
      { name: 'name', type: 'nvarchar' },
    ],
    rows: [['1', 'Test Item 1']],
    totalRows: 3,
    filteredRows: 1,
    simdAvailable: true,
  },
  getDatabases: ['master', 'tempdb', 'model', 'msdb'],
  getTables: [
    { schema: 'dbo', name: 'Users', type: 'TABLE' },
    { schema: 'dbo', name: 'Orders', type: 'TABLE' },
    { schema: 'dbo', name: 'Products', type: 'TABLE' },
  ],
  getColumns: [
    { name: 'id', type: 'int', size: 4, nullable: false, isPrimaryKey: true },
    { name: 'name', type: 'nvarchar', size: 255, nullable: false, isPrimaryKey: false },
    { name: 'created_at', type: 'datetime', size: 8, nullable: true, isPrimaryKey: false },
  ],
  formatSQL: { sql: 'SELECT\n    *\nFROM\n    users\nWHERE\n    id = 1;' },
  getQueryHistory: [],
  parseA5ER: { name: '', databaseType: '', tables: [], relations: [] },
  getExecutionPlan: { plan: 'Mock execution plan text', actual: false },
  getSettings: {
    general: {
      autoConnect: false,
      lastConnectionId: '',
      confirmOnExit: true,
      maxQueryHistory: 1000,
      maxRecentConnections: 10,
      language: 'en',
    },
    editor: {
      fontSize: 14,
      fontFamily: 'Consolas',
      wordWrap: false,
      tabSize: 4,
      insertSpaces: true,
      showLineNumbers: true,
      showMinimap: true,
      theme: 'vs-dark',
    },
    grid: {
      defaultPageSize: 100,
      showRowNumbers: true,
      enableCellEditing: false,
      dateFormat: 'yyyy-MM-dd HH:mm:ss',
      nullDisplay: '(NULL)',
    },
  },
  updateSettings: { saved: true },
  getConnectionProfiles: [],
  saveConnectionProfile: { id: 'mock-profile-1' },
  deleteConnectionProfile: { deleted: true },
  getSessionState: {
    activeConnectionId: '',
    activeTabId: '',
    windowX: 100,
    windowY: 100,
    windowWidth: 1280,
    windowHeight: 720,
    isMaximized: false,
    leftPanelWidth: 250,
    bottomPanelHeight: 200,
    openTabs: [],
    expandedTreeNodes: [],
  },
  saveSessionState: { saved: true },
  searchObjects: [],
  quickSearch: [],
};

class Bridge {
  private async call<T>(method: string, params: Record<string, unknown> = {}): Promise<T> {
    const request: IPCRequest = {
      method,
      params: JSON.stringify(params),
    };

    if (window.invoke) {
      const responseStr = await window.invoke(JSON.stringify(request));
      const response: IPCResponse<T> = JSON.parse(responseStr);

      if (!response.success) {
        throw new Error(response.error || 'Unknown error');
      }

      return response.data as T;
    }

    // Development mode: return mock data
    if (import.meta.env.DEV) {
      // Small delay to simulate network
      await new Promise((resolve) => setTimeout(resolve, 50));
      return (mockData[method] ?? {}) as T;
    }

    throw new Error('Backend not available');
  }

  // Connection methods
  async connect(connectionInfo: {
    server: string;
    database: string;
    username?: string;
    password?: string;
    useWindowsAuth: boolean;
  }): Promise<{ connectionId: string }> {
    return this.call('connect', connectionInfo);
  }

  async disconnect(connectionId: string): Promise<void> {
    return this.call('disconnect', { connectionId });
  }

  async testConnection(connectionInfo: {
    server: string;
    database: string;
    username?: string;
    password?: string;
    useWindowsAuth: boolean;
  }): Promise<{ success: boolean; message: string }> {
    return this.call('testConnection', connectionInfo);
  }

  // Query methods
  async executeQuery(
    connectionId: string,
    sql: string,
    useCache = true
  ): Promise<{
    columns: { name: string; type: string }[];
    rows: string[][];
    affectedRows: number;
    executionTimeMs: number;
    cached: boolean;
  }> {
    return this.call('executeQuery', { connectionId, sql, useCache });
  }

  async cancelQuery(connectionId: string): Promise<void> {
    return this.call('cancelQuery', { connectionId });
  }

  // Schema methods
  async getDatabases(connectionId: string): Promise<string[]> {
    return this.call('getDatabases', { connectionId });
  }

  async getTables(
    connectionId: string,
    database: string
  ): Promise<
    {
      schema: string;
      name: string;
      type: string;
    }[]
  > {
    return this.call('getTables', { connectionId, database });
  }

  async getColumns(
    connectionId: string,
    table: string
  ): Promise<
    {
      name: string;
      type: string;
      size: number;
      nullable: boolean;
      isPrimaryKey: boolean;
    }[]
  > {
    return this.call('getColumns', { connectionId, table });
  }

  // Transaction methods
  async beginTransaction(connectionId: string): Promise<void> {
    return this.call('beginTransaction', { connectionId });
  }

  async commit(connectionId: string): Promise<void> {
    return this.call('commit', { connectionId });
  }

  async rollback(connectionId: string): Promise<void> {
    return this.call('rollback', { connectionId });
  }

  // Export methods
  async exportCSV(data: unknown, filepath: string): Promise<void> {
    return this.call('exportCSV', { data, filepath });
  }

  async exportJSON(data: unknown, filepath: string): Promise<void> {
    return this.call('exportJSON', { data, filepath });
  }

  async exportExcel(data: unknown, filepath: string): Promise<void> {
    return this.call('exportExcel', { data, filepath });
  }

  // SQL methods
  async formatSQL(sql: string): Promise<{ sql: string }> {
    return this.call('formatSQL', { sql });
  }

  // History methods
  async getQueryHistory(): Promise<
    {
      id: string;
      sql: string;
      timestamp: number;
      executionTimeMs: number;
      success: boolean;
    }[]
  > {
    return this.call('getQueryHistory', {});
  }

  // A5:ER methods
  async parseA5ER(filepath: string): Promise<{
    name: string;
    databaseType: string;
    tables: {
      name: string;
      logicalName: string;
      comment: string;
      columns: {
        name: string;
        logicalName: string;
        type: string;
        size: number;
        scale: number;
        nullable: boolean;
        isPrimaryKey: boolean;
        defaultValue: string;
        comment: string;
      }[];
      indexes: {
        name: string;
        columns: string[];
        isUnique: boolean;
      }[];
      posX: number;
      posY: number;
    }[];
    relations: {
      name: string;
      parentTable: string;
      childTable: string;
      parentColumn: string;
      childColumn: string;
      cardinality: string;
    }[];
  }> {
    return this.call('parseA5ER', { filepath });
  }

  // Execution plan methods
  async getExecutionPlan(
    connectionId: string,
    sql: string,
    actual = false
  ): Promise<{ plan: string; actual: boolean }> {
    return this.call('getExecutionPlan', { connectionId, sql, actual });
  }

  // Cache methods
  async getCacheStats(): Promise<{
    currentSizeBytes: number;
    maxSizeBytes: number;
    usagePercent: number;
  }> {
    return this.call('getCacheStats', {});
  }

  async clearCache(): Promise<{ cleared: boolean }> {
    return this.call('clearCache', {});
  }

  // Async query methods
  async executeAsyncQuery(connectionId: string, sql: string): Promise<{ queryId: string }> {
    return this.call('executeAsyncQuery', { connectionId, sql });
  }

  async getAsyncQueryResult(queryId: string): Promise<{
    queryId: string;
    status: 'pending' | 'running' | 'completed' | 'cancelled' | 'failed';
    error?: string;
    columns?: { name: string; type: string }[];
    rows?: string[][];
    affectedRows?: number;
    executionTimeMs?: number;
  }> {
    return this.call('getAsyncQueryResult', { queryId });
  }

  async cancelAsyncQuery(queryId: string): Promise<{ cancelled: boolean }> {
    return this.call('cancelAsyncQuery', { queryId });
  }

  async getActiveQueries(): Promise<string[]> {
    return this.call('getActiveQueries', {});
  }

  // SIMD filter methods
  async filterResultSet(
    connectionId: string,
    sql: string,
    columnIndex: number,
    filterType: 'equals' | 'contains' | 'range',
    filterValue: string,
    filterValueMax?: string
  ): Promise<{
    columns: { name: string; type: string }[];
    rows: string[][];
    totalRows: number;
    filteredRows: number;
    simdAvailable: boolean;
  }> {
    return this.call('filterResultSet', {
      connectionId,
      sql,
      columnIndex,
      filterType,
      filterValue,
      filterValueMax,
    });
  }

  // Settings methods
  async getSettings(): Promise<{
    general: {
      autoConnect: boolean;
      lastConnectionId: string;
      confirmOnExit: boolean;
      maxQueryHistory: number;
      maxRecentConnections: number;
      language: string;
    };
    editor: {
      fontSize: number;
      fontFamily: string;
      wordWrap: boolean;
      tabSize: number;
      insertSpaces: boolean;
      showLineNumbers: boolean;
      showMinimap: boolean;
      theme: string;
    };
    grid: {
      defaultPageSize: number;
      showRowNumbers: boolean;
      enableCellEditing: boolean;
      dateFormat: string;
      nullDisplay: string;
    };
  }> {
    return this.call('getSettings', {});
  }

  async updateSettings(settings: {
    general?: Partial<{
      autoConnect: boolean;
      confirmOnExit: boolean;
      maxQueryHistory: number;
      language: string;
    }>;
    editor?: Partial<{
      fontSize: number;
      fontFamily: string;
      wordWrap: boolean;
      tabSize: number;
      theme: string;
    }>;
    grid?: Partial<{
      defaultPageSize: number;
      showRowNumbers: boolean;
      nullDisplay: string;
    }>;
  }): Promise<{ saved: boolean }> {
    return this.call('updateSettings', settings);
  }

  // Connection profile methods
  async getConnectionProfiles(): Promise<
    {
      id: string;
      name: string;
      server: string;
      database: string;
      username: string;
      useWindowsAuth: boolean;
    }[]
  > {
    return this.call('getConnectionProfiles', {});
  }

  async saveConnectionProfile(profile: {
    id?: string;
    name: string;
    server: string;
    database: string;
    username?: string;
    useWindowsAuth: boolean;
  }): Promise<{ id: string }> {
    return this.call('saveConnectionProfile', profile);
  }

  async deleteConnectionProfile(id: string): Promise<{ deleted: boolean }> {
    return this.call('deleteConnectionProfile', { id });
  }

  // Session methods
  async getSessionState(): Promise<{
    activeConnectionId: string;
    activeTabId: string;
    windowX: number;
    windowY: number;
    windowWidth: number;
    windowHeight: number;
    isMaximized: boolean;
    leftPanelWidth: number;
    bottomPanelHeight: number;
    openTabs: {
      id: string;
      title: string;
      content: string;
      filePath: string;
      isDirty: boolean;
      cursorLine: number;
      cursorColumn: number;
    }[];
    expandedTreeNodes: string[];
  }> {
    return this.call('getSessionState', {});
  }

  async saveSessionState(state: {
    activeConnectionId?: string;
    activeTabId?: string;
    windowX?: number;
    windowY?: number;
    windowWidth?: number;
    windowHeight?: number;
    isMaximized?: boolean;
    leftPanelWidth?: number;
    bottomPanelHeight?: number;
    openTabs?: {
      id: string;
      title: string;
      content: string;
      filePath: string;
      isDirty: boolean;
      cursorLine: number;
      cursorColumn: number;
    }[];
    expandedTreeNodes?: string[];
  }): Promise<{ saved: boolean }> {
    return this.call('saveSessionState', state);
  }

  // Search methods
  async searchObjects(
    connectionId: string,
    pattern: string,
    options?: {
      searchTables?: boolean;
      searchViews?: boolean;
      searchProcedures?: boolean;
      searchFunctions?: boolean;
      searchColumns?: boolean;
      caseSensitive?: boolean;
      maxResults?: number;
    }
  ): Promise<
    {
      objectType: string;
      schemaName: string;
      objectName: string;
      parentName: string;
    }[]
  > {
    return this.call('searchObjects', { connectionId, pattern, ...options });
  }

  async quickSearch(connectionId: string, prefix: string, limit = 20): Promise<string[]> {
    return this.call('quickSearch', { connectionId, prefix, limit });
  }
}

export const bridge = new Bridge();
