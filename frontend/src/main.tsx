import { AllCommunityModule, ModuleRegistry } from 'ag-grid-community';
import React from 'react';
import ReactDOM from 'react-dom/client';
import App from './App';
import './styles/global.css';

// Register AG Grid modules (required for v33+)
ModuleRegistry.registerModules([AllCommunityModule]);

const rootElement = document.getElementById('root');
if (!rootElement) {
  throw new Error('Root element not found');
}

ReactDOM.createRoot(rootElement).render(
  <React.StrictMode>
    <App />
  </React.StrictMode>
);
