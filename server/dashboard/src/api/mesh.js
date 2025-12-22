const API_BASE = '/api/v1';

export async function fetchNodes() {
  const response = await fetch(`${API_BASE}/nodes`);
  if (!response.ok) throw new Error('Failed to fetch nodes');
  return response.json();
}

export async function fetchNode(nodeId) {
  const response = await fetch(`${API_BASE}/nodes/${nodeId}`);
  if (!response.ok) throw new Error('Failed to fetch node');
  return response.json();
}

export async function fetchState() {
  const response = await fetch(`${API_BASE}/state`);
  if (!response.ok) throw new Error('Failed to fetch state');
  return response.json();
}

export async function fetchNodeHistory(nodeId, hours = 24) {
  const response = await fetch(`${API_BASE}/nodes/${nodeId}/history?hours=${hours}`);
  if (!response.ok) throw new Error('Failed to fetch history');
  return response.json();
}

export async function deleteNode(nodeId) {
  const response = await fetch(`${API_BASE}/nodes/${nodeId}`, {
    method: 'DELETE',
  });
  if (!response.ok) throw new Error('Failed to delete node');
  return response.json();
}

export async function renameNode(nodeId, name) {
  const response = await fetch(`${API_BASE}/nodes/${nodeId}/name`, {
    method: 'PUT',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ name }),
  });
  if (!response.ok) throw new Error('Failed to rename node');
  return response.json();
}
