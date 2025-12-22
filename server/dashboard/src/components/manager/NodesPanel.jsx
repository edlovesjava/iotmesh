import { useState } from 'react';
import { deleteNode, renameNode } from '../../api/mesh';
import { useToast } from '../common/Toast';
import { ConfirmModal } from '../common/Modal';
import Modal from '../common/Modal';
import Badge from '../common/Badge';

function NodesPanel({ nodes = [], onRefresh }) {
  const [selectedNode, setSelectedNode] = useState(null);
  const [deleteTarget, setDeleteTarget] = useState(null);
  const [renameTarget, setRenameTarget] = useState(null);
  const [newName, setNewName] = useState('');
  const toast = useToast();

  const handleDelete = async () => {
    if (!deleteTarget) return;
    try {
      await deleteNode(deleteTarget.id);
      toast.success(`Deleted node ${deleteTarget.name}`);
      setSelectedNode(null);
      onRefresh();
    } catch (err) {
      toast.error('Failed to delete: ' + err.message);
    }
    setDeleteTarget(null);
  };

  const handleRename = async () => {
    if (!renameTarget || !newName.trim()) return;
    try {
      await renameNode(renameTarget.id, newName.trim());
      toast.success(`Renamed to ${newName.trim()}`);
      onRefresh();
    } catch (err) {
      toast.error('Failed to rename: ' + err.message);
    }
    setRenameTarget(null);
    setNewName('');
  };

  const openRenameModal = (node) => {
    setRenameTarget(node);
    setNewName(node.name);
  };

  const formatUptime = (firstSeen, lastSeen) => {
    if (!lastSeen) return '--';
    const diff = new Date(lastSeen) - new Date(firstSeen);
    const days = Math.floor(diff / (1000 * 60 * 60 * 24));
    const hours = Math.floor((diff % (1000 * 60 * 60 * 24)) / (1000 * 60 * 60));
    const mins = Math.floor((diff % (1000 * 60 * 60)) / (1000 * 60));

    if (days > 0) return `${days}d ${hours}h`;
    if (hours > 0) return `${hours}h ${mins}m`;
    return `${mins}m`;
  };

  return (
    <div className="p-6 space-y-6">
      <div className="flex items-center justify-between">
        <h2 className="text-lg font-semibold text-white">Node Management</h2>
        <button
          onClick={onRefresh}
          className="p-2 text-gray-400 hover:text-white transition-colors"
          title="Refresh"
        >
          <svg className="w-5 h-5" fill="none" stroke="currentColor" viewBox="0 0 24 24">
            <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2}
              d="M4 4v5h.582m15.356 2A8.001 8.001 0 004.582 9m0 0H9m11 11v-5h-.581m0 0a8.003 8.003 0 01-15.357-2m15.357 2H15" />
          </svg>
        </button>
      </div>

      {/* Nodes List */}
      {nodes.length === 0 ? (
        <div className="text-center py-8 text-gray-400">
          No nodes registered yet. Nodes will appear when they connect to the mesh.
        </div>
      ) : (
        <div className="overflow-x-auto">
          <table className="w-full text-sm">
            <thead>
              <tr className="text-left text-gray-400 border-b border-gray-700">
                <th className="pb-3 font-medium">Name</th>
                <th className="pb-3 font-medium">Status</th>
                <th className="pb-3 font-medium">Firmware</th>
                <th className="pb-3 font-medium">Role</th>
                <th className="pb-3 font-medium">Peers</th>
                <th className="pb-3 font-medium text-right">Actions</th>
              </tr>
            </thead>
            <tbody>
              {nodes.map((node) => (
                <tr
                  key={node.id}
                  className="border-b border-gray-700/50 text-gray-300 hover:bg-gray-700/30 cursor-pointer"
                  onClick={() => setSelectedNode(node)}
                >
                  <td className="py-3 font-medium text-white">{node.name}</td>
                  <td className="py-3">
                    <span className={`inline-flex items-center gap-1.5 ${node.is_online ? 'text-green-400' : 'text-gray-500'}`}>
                      <span className={`w-2 h-2 rounded-full ${node.is_online ? 'bg-green-400' : 'bg-gray-500'}`} />
                      {node.is_online ? 'Online' : 'Offline'}
                    </span>
                  </td>
                  <td className="py-3">{node.firmware_version || '--'}</td>
                  <td className="py-3">
                    {node.role === 'COORD' ? (
                      <Badge variant="primary">COORD</Badge>
                    ) : (
                      <span className="text-gray-400">{node.role || 'NODE'}</span>
                    )}
                  </td>
                  <td className="py-3">{node.peer_count ?? '--'}</td>
                  <td className="py-3 text-right space-x-2" onClick={(e) => e.stopPropagation()}>
                    <button
                      onClick={() => openRenameModal(node)}
                      className="p-1 text-blue-400 hover:text-blue-300"
                      title="Rename"
                    >
                      <svg className="w-4 h-4" fill="none" stroke="currentColor" viewBox="0 0 24 24">
                        <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2}
                          d="M15.232 5.232l3.536 3.536m-2.036-5.036a2.5 2.5 0 113.536 3.536L6.5 21.036H3v-3.572L16.732 3.732z" />
                      </svg>
                    </button>
                    {node.role !== 'GATEWAY' && (
                      <button
                        onClick={() => setDeleteTarget(node)}
                        className="p-1 text-red-400 hover:text-red-300"
                        title="Delete"
                      >
                        <svg className="w-4 h-4" fill="none" stroke="currentColor" viewBox="0 0 24 24">
                          <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2}
                            d="M19 7l-.867 12.142A2 2 0 0116.138 21H7.862a2 2 0 01-1.995-1.858L5 7m5 4v6m4-6v6m1-10V4a1 1 0 00-1-1h-4a1 1 0 00-1 1v3M4 7h16" />
                        </svg>
                      </button>
                    )}
                  </td>
                </tr>
              ))}
            </tbody>
          </table>
        </div>
      )}

      {/* Node Details Modal */}
      <Modal
        isOpen={!!selectedNode}
        onClose={() => setSelectedNode(null)}
        title={`Node: ${selectedNode?.name}`}
      >
        {selectedNode && (
          <div className="space-y-4">
            <div className="grid grid-cols-2 gap-4 text-sm">
              <div>
                <span className="text-gray-400">Node ID</span>
                <p className="text-white font-mono text-xs break-all">{selectedNode.id}</p>
              </div>
              <div>
                <span className="text-gray-400">IP Address</span>
                <p className="text-white">{selectedNode.ip_address || '--'}</p>
              </div>
              <div>
                <span className="text-gray-400">Firmware</span>
                <p className="text-white">{selectedNode.firmware_version || '--'}</p>
              </div>
              <div>
                <span className="text-gray-400">Role</span>
                <p className="text-white">{selectedNode.role || 'NODE'}</p>
              </div>
              <div>
                <span className="text-gray-400">Status</span>
                <p className={selectedNode.is_online ? 'text-green-400' : 'text-gray-500'}>
                  {selectedNode.is_online ? 'Online' : 'Offline'}
                </p>
              </div>
              <div>
                <span className="text-gray-400">Peer Count</span>
                <p className="text-white">{selectedNode.peer_count ?? '--'}</p>
              </div>
              <div>
                <span className="text-gray-400">First Seen</span>
                <p className="text-white text-xs">
                  {selectedNode.first_seen ? new Date(selectedNode.first_seen).toLocaleString() : '--'}
                </p>
              </div>
              <div>
                <span className="text-gray-400">Last Seen</span>
                <p className="text-white text-xs">
                  {selectedNode.last_seen ? new Date(selectedNode.last_seen).toLocaleString() : '--'}
                </p>
              </div>
            </div>

            <div className="flex gap-2 pt-4 border-t border-gray-700">
              <button
                onClick={() => {
                  openRenameModal(selectedNode);
                  setSelectedNode(null);
                }}
                className="flex-1 px-4 py-2 bg-blue-600 hover:bg-blue-700 text-white rounded-lg transition-colors"
              >
                Rename
              </button>
              {selectedNode.role !== 'GATEWAY' && (
                <button
                  onClick={() => {
                    setDeleteTarget(selectedNode);
                    setSelectedNode(null);
                  }}
                  className="flex-1 px-4 py-2 bg-red-600 hover:bg-red-700 text-white rounded-lg transition-colors"
                >
                  Delete
                </button>
              )}
            </div>
          </div>
        )}
      </Modal>

      {/* Rename Modal */}
      <Modal
        isOpen={!!renameTarget}
        onClose={() => { setRenameTarget(null); setNewName(''); }}
        title="Rename Node"
        footer={
          <>
            <button
              onClick={() => { setRenameTarget(null); setNewName(''); }}
              className="px-4 py-2 text-sm font-medium text-gray-300 hover:text-white transition-colors"
            >
              Cancel
            </button>
            <button
              onClick={handleRename}
              disabled={!newName.trim()}
              className="px-4 py-2 text-sm font-medium bg-blue-600 hover:bg-blue-700 disabled:bg-gray-600 text-white rounded-lg transition-colors"
            >
              Rename
            </button>
          </>
        }
      >
        <div className="space-y-4">
          <p className="text-gray-400 text-sm">
            Enter a new name for {renameTarget?.name}:
          </p>
          <input
            type="text"
            value={newName}
            onChange={(e) => setNewName(e.target.value)}
            placeholder="Node name"
            className="w-full px-3 py-2 bg-gray-700 border border-gray-600 rounded-lg text-white focus:outline-none focus:ring-2 focus:ring-blue-500"
            autoFocus
            onKeyDown={(e) => {
              if (e.key === 'Enter' && newName.trim()) handleRename();
            }}
          />
        </div>
      </Modal>

      {/* Delete Confirmation Modal */}
      <ConfirmModal
        isOpen={!!deleteTarget}
        onClose={() => setDeleteTarget(null)}
        onConfirm={handleDelete}
        title="Delete Node"
        message={`Are you sure you want to delete ${deleteTarget?.name}? This will remove all telemetry data for this node. The node will reappear if it reconnects to the mesh.`}
        confirmLabel="Delete"
        danger
      />
    </div>
  );
}

export default NodesPanel;
