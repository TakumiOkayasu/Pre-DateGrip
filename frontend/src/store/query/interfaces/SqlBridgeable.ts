export interface SqlBridgeable {
  formatSQL(sql: string): Promise<{ sql: string }>;
}
