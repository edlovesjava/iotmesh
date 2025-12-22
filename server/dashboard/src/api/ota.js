const API_BASE = '/api/v1';

export async function createOTAUpdate(firmwareId, forceUpdate = false, targetNodeId = null) {
  const response = await fetch(`${API_BASE}/ota/updates`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({
      firmware_id: firmwareId,
      force_update: forceUpdate,
      target_node_id: targetNodeId,
    }),
  });
  if (!response.ok) {
    const error = await response.text();
    throw new Error(error || 'Failed to create update');
  }
  return response.json();
}

export async function fetchOTAUpdates(status = null) {
  const params = status ? `?status=${status}` : '';
  const response = await fetch(`${API_BASE}/ota/updates${params}`);
  if (!response.ok) throw new Error('Failed to fetch updates');
  return response.json();
}

export async function fetchOTAUpdateDetails(updateId) {
  const response = await fetch(`${API_BASE}/ota/updates/${updateId}`);
  if (!response.ok) throw new Error('Failed to fetch update details');
  return response.json();
}

export async function cancelOTAUpdate(updateId) {
  const response = await fetch(`${API_BASE}/ota/updates/${updateId}`, {
    method: 'DELETE',
  });
  if (!response.ok) {
    const error = await response.text();
    throw new Error(error || 'Failed to cancel update');
  }
  return response.json();
}

export function getUpdateStatusColor(status) {
  const colors = {
    pending: 'yellow',
    distributing: 'blue',
    completed: 'green',
    failed: 'red',
    cancelled: 'gray',
  };
  return colors[status] || 'gray';
}
