const API_BASE = '/api/v1';

export async function uploadFirmware(formData) {
  const response = await fetch(`${API_BASE}/firmware/upload`, {
    method: 'POST',
    body: formData,
  });
  if (!response.ok) {
    const error = await response.text();
    throw new Error(error || 'Failed to upload firmware');
  }
  return response.json();
}

export async function fetchFirmwareList(nodeType = null) {
  const params = nodeType ? `?node_type=${nodeType}` : '';
  const response = await fetch(`${API_BASE}/firmware${params}`);
  if (!response.ok) throw new Error('Failed to fetch firmware');
  return response.json();
}

export async function fetchFirmware(firmwareId) {
  const response = await fetch(`${API_BASE}/firmware/${firmwareId}`);
  if (!response.ok) throw new Error('Failed to fetch firmware');
  return response.json();
}

export async function deleteFirmware(firmwareId) {
  const response = await fetch(`${API_BASE}/firmware/${firmwareId}`, {
    method: 'DELETE',
  });
  if (!response.ok) throw new Error('Failed to delete firmware');
  return response.json();
}

export async function toggleFirmwareStable(firmwareId, isStable) {
  const response = await fetch(`${API_BASE}/firmware/${firmwareId}/stable?is_stable=${isStable}`, {
    method: 'PATCH',
  });
  if (!response.ok) throw new Error('Failed to update firmware');
  return response.json();
}

export function getFirmwareDownloadUrl(firmwareId) {
  return `${API_BASE}/firmware/${firmwareId}/download`;
}

export function formatFileSize(bytes) {
  if (bytes < 1024) return `${bytes} B`;
  if (bytes < 1024 * 1024) return `${(bytes / 1024).toFixed(1)} KB`;
  return `${(bytes / (1024 * 1024)).toFixed(2)} MB`;
}
