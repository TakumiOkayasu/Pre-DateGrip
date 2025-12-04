import { AllCommunityModule, ModuleRegistry, themeQuartz } from 'ag-grid-community';
import React from 'react';
import ReactDOM from 'react-dom/client';
import App from './App';
import './styles/global.css';

// Register AG Grid modules (required for v33+)
ModuleRegistry.registerModules([AllCommunityModule]);

// Create dark theme for AG Grid
export const darkTheme = themeQuartz.withParams({
  backgroundColor: '#2b2b2b',
  foregroundColor: '#bbbbbb',
  headerBackgroundColor: '#3c3f41',
  headerTextColor: '#bbbbbb',
  oddRowBackgroundColor: '#313335',
  rowHoverColor: '#4e5254',
  selectedRowBackgroundColor: '#214283',
  borderColor: '#323232',
  chromeBackgroundColor: '#3c3f41',
  headerColumnResizeHandleColor: '#4a88c7',
  accentColor: '#4a88c7',
});

const rootElement = document.getElementById('root');
if (!rootElement) {
  throw new Error('Root element not found');
}

ReactDOM.createRoot(rootElement).render(
  <React.StrictMode>
    <App />
  </React.StrictMode>
);
