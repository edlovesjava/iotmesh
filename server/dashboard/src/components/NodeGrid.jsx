import { formatDistanceToNow } from '../utils/time';

function NodeCard({ node }) {
  const isOnline = node.is_online;

  return (
    <div className={`rounded-lg p-4 border ${
      isOnline
        ? 'bg-gray-800 border-green-500/50'
        : 'bg-gray-800/50 border-gray-700'
    }`}>
      <div className="flex items-center justify-between mb-2">
        <h3 className="font-mono text-lg font-semibold">
          {node.name || node.id}
        </h3>
        <span className={`px-2 py-0.5 rounded text-xs font-medium ${
          isOnline
            ? 'bg-green-500/20 text-green-400'
            : 'bg-gray-600/20 text-gray-500'
        }`}>
          {isOnline ? 'ONLINE' : 'OFFLINE'}
        </span>
      </div>

      <div className="text-sm text-gray-400 space-y-1">
        <div className="flex justify-between">
          <span>ID:</span>
          <span className="font-mono text-gray-300">{node.id}</span>
        </div>
        <div className="flex justify-between">
          <span>Role:</span>
          <span className={`font-medium ${
            node.role === 'COORD' ? 'text-yellow-400' : 'text-gray-300'
          }`}>
            {node.role || 'NODE'}
          </span>
        </div>
        {node.firmware_version && (
          <div className="flex justify-between">
            <span>Firmware:</span>
            <span className="text-gray-300">{node.firmware_version}</span>
          </div>
        )}
        <div className="flex justify-between">
          <span>Last seen:</span>
          <span className="text-gray-300">
            {node.last_seen ? formatDistanceToNow(node.last_seen) : 'Never'}
          </span>
        </div>
      </div>
    </div>
  );
}

export default function NodeGrid({ nodes, loading, error }) {
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
        Error loading nodes: {error}
      </div>
    );
  }

  if (!nodes || nodes.length === 0) {
    return (
      <div className="bg-gray-800 rounded-lg p-8 text-center text-gray-500">
        No nodes registered yet. Start a gateway node to begin collecting telemetry.
      </div>
    );
  }

  const onlineCount = nodes.filter(n => n.is_online).length;

  return (
    <div>
      <div className="flex items-center justify-between mb-4">
        <h2 className="text-xl font-semibold">
          Nodes
          <span className="ml-2 text-sm font-normal text-gray-500">
            ({onlineCount}/{nodes.length} online)
          </span>
        </h2>
      </div>
      <div className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-3 xl:grid-cols-4 gap-4">
        {nodes.map((node) => (
          <NodeCard key={node.id} node={node} />
        ))}
      </div>
    </div>
  );
}
