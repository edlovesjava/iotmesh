import { formatDistanceToNow } from '../utils/time';

export default function StateTable({ state, loading, error }) {
  if (loading) {
    return (
      <div className="flex items-center justify-center h-32">
        <div className="animate-spin rounded-full h-8 w-8 border-b-2 border-blue-500"></div>
      </div>
    );
  }

  if (error) {
    return (
      <div className="bg-red-500/10 border border-red-500/50 rounded-lg p-4 text-red-400">
        Error loading state: {error}
      </div>
    );
  }

  if (!state || state.length === 0) {
    return (
      <div className="bg-gray-800 rounded-lg p-8 text-center text-gray-500">
        No state data available yet.
      </div>
    );
  }

  // Group state by key for easier viewing
  const stateByKey = state.reduce((acc, item) => {
    if (!acc[item.key]) {
      acc[item.key] = [];
    }
    acc[item.key].push(item);
    return acc;
  }, {});

  return (
    <div>
      <h2 className="text-xl font-semibold mb-4">Shared State</h2>
      <div className="bg-gray-800 rounded-lg overflow-hidden">
        <table className="w-full">
          <thead>
            <tr className="bg-gray-700/50 text-left text-sm text-gray-400">
              <th className="px-4 py-3 font-medium">Key</th>
              <th className="px-4 py-3 font-medium">Value</th>
              <th className="px-4 py-3 font-medium">Node</th>
              <th className="px-4 py-3 font-medium">Version</th>
              <th className="px-4 py-3 font-medium">Updated</th>
            </tr>
          </thead>
          <tbody className="divide-y divide-gray-700/50">
            {state.map((item, index) => (
              <tr key={`${item.node_id}-${item.key}`} className="hover:bg-gray-700/30">
                <td className="px-4 py-3">
                  <span className={`font-mono ${
                    item.key.startsWith('motion') ? 'text-orange-400' :
                    item.key.startsWith('temp') ? 'text-blue-400' :
                    item.key.startsWith('humidity') ? 'text-cyan-400' :
                    item.key === 'led' ? 'text-yellow-400' :
                    'text-gray-300'
                  }`}>
                    {item.key}
                  </span>
                </td>
                <td className="px-4 py-3">
                  <span className={`font-mono font-semibold ${
                    item.value === '1' ? 'text-green-400' :
                    item.value === '0' ? 'text-gray-500' :
                    'text-white'
                  }`}>
                    {item.value}
                  </span>
                </td>
                <td className="px-4 py-3 font-mono text-sm text-gray-400">
                  {item.node_id}
                </td>
                <td className="px-4 py-3 text-sm text-gray-500">
                  v{item.version}
                </td>
                <td className="px-4 py-3 text-sm text-gray-500">
                  {item.updated_at ? formatDistanceToNow(item.updated_at) : '--'}
                </td>
              </tr>
            ))}
          </tbody>
        </table>
      </div>
    </div>
  );
}
