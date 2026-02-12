import type { HistoryItem } from '../../../types';

export interface HistoryRecordable {
  addHistory(item: Omit<HistoryItem, 'id'>): void;
}
