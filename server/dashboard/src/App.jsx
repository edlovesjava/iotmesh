import { useState, useEffect, useCallback } from 'react';
import NodeGrid from './components/NodeGrid';
import StateTable from './components/StateTable';
import { fetchNodes, fetchState } from './api/mesh';

const REFRESH_INTERVAL = 5000; // 5 seconds

function Header({ lastUpdate, refreshing, onRefresh }) {
  return (
    <header className="bg-gray-800 border-b border-gray-700">
      <div className="max-w-7xl mx-auto px-4 py-4">
        <div className="flex items-center justify-between">
          <div className="flex items-center gap-3">
            <div className="w-10 h-10 bg-blue-500 rounded-lg flex items-center justify-center">
              <svg className="w-6 h-6 text-white" fill="none" stroke="currentColor" viewBox="0 0 24 24">
                <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2}
                  d="M9 3v2m6-2v2M9 19v2m6-2v2M5 9H3m2 6H3m18-6h-2m2 6h-2M7 19h10a2 2 0 002-2V7a2 2 0 00-2-2H7a2 2 0 00-2 2v10a2 2 0 002 2zM9 9h6v6H9V9z" />
              </svg>
            </div>
            <div>
              <h1 className="text-xl font-bold">IoT Mesh Dashboard</h1>
              <p className="text-sm text-gray-400">Real-time mesh network monitoring</p>
            </div>
          </div>
          <div className="flex items-center gap-4">
            <span className="text-sm text-gray-500">
              {lastUpdate ? `Updated ${new Date(lastUpdate).toLocaleTimeString()}` : 'Loading...'}
            </span>
            <button
              onClick={onRefresh}
              disabled={refreshing}
              className="px-3 py-1.5 bg-blue-600 hover:bg-blue-700 disabled:bg-blue-800
                         rounded-lg text-sm font-medium transition-colors flex items-center gap-2"
            >
              <svg className={`w-4 h-4 ${refreshing ? 'animate-spin' : ''}`} fill="none" stroke="currentColor" viewBox="0 0 24 24">
                <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2}
                  d="M4 4v5h.582m15.356 2A8.001 8.001 0 004.582 9m0 0H9m11 11v-5h-.581m0 0a8.003 8.003 0 01-15.357-2m15.357 2H15" />
              </svg>
              Refresh
            </button>
          </div>
        </div>
      </div>
    </header>
  );
}

function App() {
  const [nodes, setNodes] = useState([]);
  const [state, setState] = useState([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState(null);
  const [lastUpdate, setLastUpdate] = useState(null);
  const [refreshing, setRefreshing] = useState(false);

  const loadData = useCallback(async (showRefreshing = false) => {
    if (showRefreshing) setRefreshing(true);

    try {
      const [nodesData, stateData] = await Promise.all([
        fetchNodes(),
        fetchState(),
      ]);

      setNodes(nodesData);
      setState(stateData);
      setError(null);
      setLastUpdate(new Date().toISOString());
    } catch (err) {
      console.error('Failed to load data:', err);
      setError(err.message);
    } finally {
      setLoading(false);
      setRefreshing(false);
    }
  }, []);

  // Initial load
  useEffect(() => {
    loadData();
  }, [loadData]);

  // Auto-refresh
  useEffect(() => {
    const interval = setInterval(() => loadData(), REFRESH_INTERVAL);
    return () => clearInterval(interval);
  }, [loadData]);

  const handleRefresh = () => loadData(true);

  return (
    <div className="min-h-screen bg-gray-900">
      <Header
        lastUpdate={lastUpdate}
        refreshing={refreshing}
        onRefresh={handleRefresh}
      />

      <main className="max-w-7xl mx-auto px-4 py-8 space-y-8">
        {error && !loading && (
          <div className="bg-yellow-500/10 border border-yellow-500/50 rounded-lg p-4 text-yellow-400">
            <strong>Connection issue:</strong> {error}
            <br />
            <span className="text-sm">Make sure the API server is running on port 8000.</span>
          </div>
        )}

        <NodeGrid
          nodes={nodes}
          loading={loading}
          error={null}
        />

        <StateTable
          state={state}
          loading={loading}
          error={null}
        />
      </main>

      <footer className="border-t border-gray-800 mt-auto">
        <div className="max-w-7xl mx-auto px-4 py-4 text-center text-sm text-gray-600">
          IoT Mesh Dashboard - Refreshes every {REFRESH_INTERVAL / 1000} seconds
        </div>
      </footer>
    </div>
  );
}

export default App;
