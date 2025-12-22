import { useState, useEffect, useCallback } from 'react';
import { fetchFirmwareList } from '../../api/firmware';
import { createOTAUpdate, fetchOTAUpdates, fetchOTAUpdateDetails, cancelOTAUpdate } from '../../api/ota';
import { useToast } from '../common/Toast';
import { ConfirmModal } from '../common/Modal';
import ProgressBar from '../common/ProgressBar';
import { StatusBadge } from '../common/Badge';

function OTAPanel() {
  const [firmware, setFirmware] = useState([]);
  const [updates, setUpdates] = useState([]);
  const [activeUpdates, setActiveUpdates] = useState([]);
  const [loading, setLoading] = useState(true);
  const [cancelTarget, setCancelTarget] = useState(null);
  const toast = useToast();

  const loadData = useCallback(async () => {
    try {
      const [fwData, updateData] = await Promise.all([
        fetchFirmwareList(),
        fetchOTAUpdates(),
      ]);
      setFirmware(fwData.items || []);
      setUpdates(updateData || []);

      // Find active updates (pending or distributing) and get their details
      const active = updateData.filter(u => u.status === 'pending' || u.status === 'distributing');
      if (active.length > 0) {
        const detailPromises = active.map(u => fetchOTAUpdateDetails(u.id));
        const details = await Promise.all(detailPromises);
        setActiveUpdates(details);
      } else {
        setActiveUpdates([]);
      }
    } catch (err) {
      toast.error('Failed to load data: ' + err.message);
    } finally {
      setLoading(false);
    }
  }, [toast]);

  useEffect(() => {
    loadData();
  }, [loadData]);

  // Poll for active updates
  useEffect(() => {
    if (activeUpdates.length === 0) return;

    const interval = setInterval(async () => {
      try {
        const detailPromises = activeUpdates.map(u => fetchOTAUpdateDetails(u.id));
        const details = await Promise.all(detailPromises);

        // Check if any completed
        const stillActive = details.filter(d => d.status === 'pending' || d.status === 'distributing');
        setActiveUpdates(stillActive);

        // If some completed, refresh the full list
        if (stillActive.length < activeUpdates.length) {
          loadData();
        }
      } catch (err) {
        console.error('Polling error:', err);
      }
    }, 3000);

    return () => clearInterval(interval);
  }, [activeUpdates, loadData]);

  const handleCancel = async () => {
    if (!cancelTarget) return;
    try {
      await cancelOTAUpdate(cancelTarget.id);
      toast.success('Update cancelled');
      loadData();
    } catch (err) {
      toast.error('Failed to cancel: ' + err.message);
    }
    setCancelTarget(null);
  };

  return (
    <div className="p-6 space-y-6">
      <h2 className="text-lg font-semibold text-white">OTA Updates</h2>

      {/* Create Update Form */}
      <OTACreateForm firmware={firmware} onSuccess={loadData} />

      {/* Active Updates */}
      {activeUpdates.length > 0 && (
        <div className="space-y-4">
          <h3 className="text-md font-medium text-gray-300">Active Updates</h3>
          {activeUpdates.map((update) => (
            <OTAProgressCard
              key={update.id}
              update={update}
              onCancel={() => setCancelTarget(update)}
            />
          ))}
        </div>
      )}

      {/* Update History */}
      <div className="space-y-4">
        <h3 className="text-md font-medium text-gray-300">Update History</h3>

        {loading ? (
          <div className="text-center py-8 text-gray-400">Loading updates...</div>
        ) : updates.length === 0 ? (
          <div className="text-center py-8 text-gray-400">
            No OTA updates yet. Create your first update above.
          </div>
        ) : (
          <div className="overflow-x-auto">
            <table className="w-full text-sm">
              <thead>
                <tr className="text-left text-gray-400 border-b border-gray-700">
                  <th className="pb-3 font-medium">ID</th>
                  <th className="pb-3 font-medium">Type</th>
                  <th className="pb-3 font-medium">Status</th>
                  <th className="pb-3 font-medium">Created</th>
                  <th className="pb-3 font-medium text-right">Actions</th>
                </tr>
              </thead>
              <tbody>
                {updates.map((update) => (
                  <tr key={update.id} className="border-b border-gray-700/50 text-gray-300">
                    <td className="py-3">#{update.id}</td>
                    <td className="py-3">{update.target_node_type}</td>
                    <td className="py-3">
                      <StatusBadge status={update.status} />
                    </td>
                    <td className="py-3">
                      {new Date(update.created_at).toLocaleString()}
                    </td>
                    <td className="py-3 text-right">
                      {update.status === 'pending' && (
                        <button
                          onClick={() => setCancelTarget(update)}
                          className="text-red-400 hover:text-red-300 text-sm"
                        >
                          Cancel
                        </button>
                      )}
                    </td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
        )}
      </div>

      {/* Cancel Confirmation Modal */}
      <ConfirmModal
        isOpen={!!cancelTarget}
        onClose={() => setCancelTarget(null)}
        onConfirm={handleCancel}
        title="Cancel Update"
        message="Are you sure you want to cancel this update? Nodes that have already received the update will keep the new firmware."
        confirmLabel="Cancel Update"
        danger
      />
    </div>
  );
}

function OTACreateForm({ firmware, onSuccess }) {
  const [selectedType, setSelectedType] = useState('');
  const [selectedFirmware, setSelectedFirmware] = useState('');
  const [forceUpdate, setForceUpdate] = useState(false);
  const [creating, setCreating] = useState(false);
  const toast = useToast();

  // Get unique node types from firmware
  const nodeTypes = [...new Set(firmware.map(fw => fw.node_type))];

  // Filter firmware by selected type
  const filteredFirmware = selectedType
    ? firmware.filter(fw => fw.node_type === selectedType)
    : [];

  const handleSubmit = async (e) => {
    e.preventDefault();

    if (!selectedFirmware) {
      toast.error('Please select firmware');
      return;
    }

    setCreating(true);
    try {
      await createOTAUpdate(parseInt(selectedFirmware), forceUpdate);
      toast.success('OTA update job created');

      // Reset form
      setSelectedType('');
      setSelectedFirmware('');
      setForceUpdate(false);

      onSuccess();
    } catch (err) {
      toast.error('Failed to create update: ' + err.message);
    } finally {
      setCreating(false);
    }
  };

  return (
    <form onSubmit={handleSubmit} className="bg-gray-700/50 rounded-lg p-4 space-y-4">
      <h3 className="text-md font-medium text-gray-300">Create Update</h3>

      <div className="grid grid-cols-1 md:grid-cols-3 gap-4">
        <div>
          <label className="block text-sm text-gray-400 mb-1">Target Type</label>
          <select
            value={selectedType}
            onChange={(e) => {
              setSelectedType(e.target.value);
              setSelectedFirmware('');
            }}
            className="w-full px-3 py-2 bg-gray-700 border border-gray-600 rounded-lg text-white focus:outline-none focus:ring-2 focus:ring-blue-500"
          >
            <option value="">Select type...</option>
            {nodeTypes.map((type) => (
              <option key={type} value={type}>{type}</option>
            ))}
          </select>
        </div>

        <div>
          <label className="block text-sm text-gray-400 mb-1">Firmware</label>
          <select
            value={selectedFirmware}
            onChange={(e) => setSelectedFirmware(e.target.value)}
            disabled={!selectedType}
            className="w-full px-3 py-2 bg-gray-700 border border-gray-600 rounded-lg text-white focus:outline-none focus:ring-2 focus:ring-blue-500 disabled:opacity-50"
          >
            <option value="">Select firmware...</option>
            {filteredFirmware.map((fw) => (
              <option key={fw.id} value={fw.id}>
                v{fw.version} {fw.is_stable ? '(stable)' : ''} - {fw.hardware}
              </option>
            ))}
          </select>
        </div>

        <div className="flex items-end">
          <button
            type="submit"
            disabled={creating || !selectedFirmware}
            className="w-full px-4 py-2 bg-blue-600 hover:bg-blue-700 disabled:bg-gray-600 disabled:cursor-not-allowed
                       text-white font-medium rounded-lg transition-colors flex items-center justify-center gap-2"
          >
            {creating ? (
              <>
                <svg className="w-4 h-4 animate-spin" fill="none" viewBox="0 0 24 24">
                  <circle className="opacity-25" cx="12" cy="12" r="10" stroke="currentColor" strokeWidth="4" />
                  <path className="opacity-75" fill="currentColor" d="M4 12a8 8 0 018-8V0C5.373 0 0 5.373 0 12h4zm2 5.291A7.962 7.962 0 014 12H0c0 3.042 1.135 5.824 3 7.938l3-2.647z" />
                </svg>
                Creating...
              </>
            ) : (
              'Create Update Job'
            )}
          </button>
        </div>
      </div>

      <div className="flex items-center gap-2">
        <label className="flex items-center gap-2 text-sm text-gray-300 cursor-pointer">
          <input
            type="checkbox"
            checked={forceUpdate}
            onChange={(e) => setForceUpdate(e.target.checked)}
            className="w-4 h-4 rounded bg-gray-700 border-gray-600 text-blue-600 focus:ring-blue-500"
          />
          Force update (ignore MD5 match)
        </label>
        <span className="text-xs text-gray-500" title="Forces update even if node already has this firmware version">
          ?
        </span>
      </div>
    </form>
  );
}

function OTAProgressCard({ update, onCancel }) {
  // Calculate overall progress
  const nodes = update.nodes || [];
  const completedNodes = nodes.filter(n => n.status === 'completed').length;
  const totalNodes = nodes.length || 1;
  const overallProgress = (completedNodes / totalNodes) * 100;

  return (
    <div className="bg-gray-700/50 rounded-lg p-4 space-y-4">
      <div className="flex items-center justify-between">
        <div className="flex items-center gap-3">
          <span className="text-white font-medium">
            Update #{update.id}: {update.node_type} → v{update.version}
          </span>
          <StatusBadge status={update.status} />
        </div>
        {update.status === 'pending' && (
          <button
            onClick={onCancel}
            className="text-sm text-red-400 hover:text-red-300"
          >
            Cancel
          </button>
        )}
      </div>

      {/* Overall Progress */}
      <div>
        <ProgressBar
          value={completedNodes}
          max={totalNodes}
          color={update.status === 'distributing' ? 'blue' : 'gray'}
          showLabel
          label={`${completedNodes}/${totalNodes} nodes`}
        />
      </div>

      {/* Per-Node Progress */}
      {nodes.length > 0 && (
        <div className="space-y-2 text-sm">
          {nodes.map((node) => (
            <div key={node.node_id} className="flex items-center gap-3">
              <span className="w-32 text-gray-400 truncate" title={node.node_id}>
                {node.node_id.slice(0, 12)}...
              </span>
              <div className="flex-1">
                <ProgressBar
                  value={node.current_part}
                  max={node.total_parts || 1}
                  size="sm"
                  color={
                    node.status === 'completed' ? 'green' :
                    node.status === 'failed' ? 'red' :
                    node.status === 'downloading' ? 'blue' : 'gray'
                  }
                />
              </div>
              <span className={`w-20 text-right ${
                node.status === 'completed' ? 'text-green-400' :
                node.status === 'failed' ? 'text-red-400' :
                'text-gray-400'
              }`}>
                {node.status === 'completed' ? '100%' :
                 node.status === 'failed' ? 'failed' :
                 node.status === 'pending' ? 'pending' :
                 `${Math.round((node.current_part / (node.total_parts || 1)) * 100)}%`
                }
              </span>
            </div>
          ))}
        </div>
      )}
    </div>
  );
}

export default OTAPanel;
