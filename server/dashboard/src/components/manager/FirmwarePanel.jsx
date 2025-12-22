import { useState, useEffect, useCallback, useRef } from 'react';
import { fetchFirmwareList, uploadFirmware, deleteFirmware, toggleFirmwareStable, getFirmwareDownloadUrl, formatFileSize } from '../../api/firmware';
import { useToast } from '../common/Toast';
import { ConfirmModal } from '../common/Modal';
import Badge from '../common/Badge';

const NODE_TYPES = ['pir', 'led', 'button', 'button2', 'dht', 'watcher', 'gateway', 'light'];
const HARDWARE_TYPES = ['ESP32', 'ESP32-S2', 'ESP32-S3', 'ESP32-C3'];

function FirmwarePanel() {
  const [firmware, setFirmware] = useState([]);
  const [loading, setLoading] = useState(true);
  const [filterType, setFilterType] = useState('');
  const [deleteTarget, setDeleteTarget] = useState(null);
  const toast = useToast();

  const loadFirmware = useCallback(async () => {
    try {
      const data = await fetchFirmwareList(filterType || null);
      setFirmware(data.items || []);
    } catch (err) {
      toast.error('Failed to load firmware: ' + err.message);
    } finally {
      setLoading(false);
    }
  }, [filterType, toast]);

  useEffect(() => {
    loadFirmware();
  }, [loadFirmware]);

  const handleDelete = async () => {
    if (!deleteTarget) return;
    try {
      await deleteFirmware(deleteTarget.id);
      toast.success(`Deleted ${deleteTarget.filename}`);
      loadFirmware();
    } catch (err) {
      toast.error('Failed to delete: ' + err.message);
    }
    setDeleteTarget(null);
  };

  const handleToggleStable = async (fw) => {
    try {
      await toggleFirmwareStable(fw.id, !fw.is_stable);
      toast.success(`${fw.node_type} v${fw.version} marked as ${fw.is_stable ? 'unstable' : 'stable'}`);
      loadFirmware();
    } catch (err) {
      toast.error('Failed to update: ' + err.message);
    }
  };

  return (
    <div className="p-6 space-y-6">
      <h2 className="text-lg font-semibold text-white">Firmware Management</h2>

      {/* Upload Form */}
      <FirmwareUploadForm onSuccess={loadFirmware} />

      {/* Firmware List */}
      <div className="space-y-4">
        <div className="flex items-center justify-between">
          <h3 className="text-md font-medium text-gray-300">Firmware Library</h3>
          <select
            value={filterType}
            onChange={(e) => setFilterType(e.target.value)}
            className="px-3 py-1.5 bg-gray-700 border border-gray-600 rounded-lg text-sm text-white focus:outline-none focus:ring-2 focus:ring-blue-500"
          >
            <option value="">All Types</option>
            {NODE_TYPES.map((type) => (
              <option key={type} value={type}>{type}</option>
            ))}
          </select>
        </div>

        {loading ? (
          <div className="text-center py-8 text-gray-400">Loading firmware...</div>
        ) : firmware.length === 0 ? (
          <div className="text-center py-8 text-gray-400">
            No firmware uploaded yet. Upload your first firmware above.
          </div>
        ) : (
          <div className="overflow-x-auto">
            <table className="w-full text-sm">
              <thead>
                <tr className="text-left text-gray-400 border-b border-gray-700">
                  <th className="pb-3 font-medium">Type</th>
                  <th className="pb-3 font-medium">Version</th>
                  <th className="pb-3 font-medium">Hardware</th>
                  <th className="pb-3 font-medium">Size</th>
                  <th className="pb-3 font-medium">Stable</th>
                  <th className="pb-3 font-medium">Date</th>
                  <th className="pb-3 font-medium text-right">Actions</th>
                </tr>
              </thead>
              <tbody>
                {firmware.map((fw) => (
                  <tr key={fw.id} className="border-b border-gray-700/50 text-gray-300">
                    <td className="py-3">
                      <Badge variant="info">{fw.node_type}</Badge>
                    </td>
                    <td className="py-3">{fw.version}</td>
                    <td className="py-3">{fw.hardware}</td>
                    <td className="py-3">{formatFileSize(fw.size_bytes)}</td>
                    <td className="py-3">
                      <button
                        onClick={() => handleToggleStable(fw)}
                        className={`text-lg ${fw.is_stable ? 'text-yellow-400' : 'text-gray-600 hover:text-gray-400'}`}
                        title={fw.is_stable ? 'Mark as unstable' : 'Mark as stable'}
                      >
                        ★
                      </button>
                    </td>
                    <td className="py-3">
                      {new Date(fw.created_at).toLocaleDateString()}
                    </td>
                    <td className="py-3 text-right space-x-2">
                      <a
                        href={getFirmwareDownloadUrl(fw.id)}
                        className="inline-flex items-center px-2 py-1 text-blue-400 hover:text-blue-300"
                        title="Download"
                      >
                        <svg className="w-4 h-4" fill="none" stroke="currentColor" viewBox="0 0 24 24">
                          <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2}
                            d="M4 16v1a3 3 0 003 3h10a3 3 0 003-3v-1m-4-4l-4 4m0 0l-4-4m4 4V4" />
                        </svg>
                      </a>
                      <button
                        onClick={() => setDeleteTarget(fw)}
                        className="inline-flex items-center px-2 py-1 text-red-400 hover:text-red-300"
                        title="Delete"
                      >
                        <svg className="w-4 h-4" fill="none" stroke="currentColor" viewBox="0 0 24 24">
                          <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2}
                            d="M19 7l-.867 12.142A2 2 0 0116.138 21H7.862a2 2 0 01-1.995-1.858L5 7m5 4v6m4-6v6m1-10V4a1 1 0 00-1-1h-4a1 1 0 00-1 1v3M4 7h16" />
                        </svg>
                      </button>
                    </td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
        )}
      </div>

      {/* Delete Confirmation Modal */}
      <ConfirmModal
        isOpen={!!deleteTarget}
        onClose={() => setDeleteTarget(null)}
        onConfirm={handleDelete}
        title="Delete Firmware"
        message={`Are you sure you want to delete ${deleteTarget?.node_type} v${deleteTarget?.version}? This action cannot be undone.`}
        confirmLabel="Delete"
        danger
      />
    </div>
  );
}

function FirmwareUploadForm({ onSuccess }) {
  const [file, setFile] = useState(null);
  const [nodeType, setNodeType] = useState('');
  const [version, setVersion] = useState('');
  const [hardware, setHardware] = useState('ESP32');
  const [releaseNotes, setReleaseNotes] = useState('');
  const [isStable, setIsStable] = useState(false);
  const [uploading, setUploading] = useState(false);
  const [dragActive, setDragActive] = useState(false);
  const fileInputRef = useRef(null);
  const toast = useToast();

  const handleDrag = (e) => {
    e.preventDefault();
    e.stopPropagation();
    if (e.type === 'dragenter' || e.type === 'dragover') {
      setDragActive(true);
    } else if (e.type === 'dragleave') {
      setDragActive(false);
    }
  };

  const handleDrop = (e) => {
    e.preventDefault();
    e.stopPropagation();
    setDragActive(false);

    if (e.dataTransfer.files && e.dataTransfer.files[0]) {
      const droppedFile = e.dataTransfer.files[0];
      if (droppedFile.name.endsWith('.bin')) {
        setFile(droppedFile);
        // Try to extract node type from filename
        const match = droppedFile.name.match(/^(\w+)_/);
        if (match && NODE_TYPES.includes(match[1])) {
          setNodeType(match[1]);
        }
      } else {
        toast.error('Please upload a .bin file');
      }
    }
  };

  const handleFileChange = (e) => {
    if (e.target.files && e.target.files[0]) {
      setFile(e.target.files[0]);
    }
  };

  const handleSubmit = async (e) => {
    e.preventDefault();

    if (!file || !nodeType || !version) {
      toast.error('Please fill in all required fields');
      return;
    }

    setUploading(true);

    try {
      const formData = new FormData();
      formData.append('file', file);
      formData.append('node_type', nodeType);
      formData.append('version', version);
      formData.append('hardware', hardware);
      formData.append('release_notes', releaseNotes);
      formData.append('is_stable', isStable);

      await uploadFirmware(formData);
      toast.success(`Uploaded ${nodeType} v${version}`);

      // Reset form
      setFile(null);
      setNodeType('');
      setVersion('');
      setReleaseNotes('');
      setIsStable(false);
      if (fileInputRef.current) fileInputRef.current.value = '';

      onSuccess();
    } catch (err) {
      toast.error('Upload failed: ' + err.message);
    } finally {
      setUploading(false);
    }
  };

  return (
    <form onSubmit={handleSubmit} className="bg-gray-700/50 rounded-lg p-4 space-y-4">
      <h3 className="text-md font-medium text-gray-300">Upload New Firmware</h3>

      {/* File Drop Zone */}
      <div
        onDragEnter={handleDrag}
        onDragLeave={handleDrag}
        onDragOver={handleDrag}
        onDrop={handleDrop}
        onClick={() => fileInputRef.current?.click()}
        className={`
          border-2 border-dashed rounded-lg p-6 text-center cursor-pointer transition-colors
          ${dragActive ? 'border-blue-500 bg-blue-500/10' : 'border-gray-600 hover:border-gray-500'}
          ${file ? 'border-green-500 bg-green-500/10' : ''}
        `}
      >
        <input
          ref={fileInputRef}
          type="file"
          accept=".bin"
          onChange={handleFileChange}
          className="hidden"
        />
        {file ? (
          <div className="text-green-400">
            <svg className="w-8 h-8 mx-auto mb-2" fill="none" stroke="currentColor" viewBox="0 0 24 24">
              <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M5 13l4 4L19 7" />
            </svg>
            <p className="font-medium">{file.name}</p>
            <p className="text-sm text-gray-400">{formatFileSize(file.size)}</p>
          </div>
        ) : (
          <div className="text-gray-400">
            <svg className="w-8 h-8 mx-auto mb-2" fill="none" stroke="currentColor" viewBox="0 0 24 24">
              <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2}
                d="M7 16a4 4 0 01-.88-7.903A5 5 0 1115.9 6L16 6a5 5 0 011 9.9M15 13l-3-3m0 0l-3 3m3-3v12" />
            </svg>
            <p>Drop firmware.bin here or click to browse</p>
          </div>
        )}
      </div>

      {/* Form Fields */}
      <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-4 gap-4">
        <div>
          <label className="block text-sm text-gray-400 mb-1">Node Type *</label>
          <select
            value={nodeType}
            onChange={(e) => setNodeType(e.target.value)}
            className="w-full px-3 py-2 bg-gray-700 border border-gray-600 rounded-lg text-white focus:outline-none focus:ring-2 focus:ring-blue-500"
            required
          >
            <option value="">Select type...</option>
            {NODE_TYPES.map((type) => (
              <option key={type} value={type}>{type}</option>
            ))}
          </select>
        </div>

        <div>
          <label className="block text-sm text-gray-400 mb-1">Version *</label>
          <input
            type="text"
            value={version}
            onChange={(e) => setVersion(e.target.value)}
            placeholder="1.0.0"
            className="w-full px-3 py-2 bg-gray-700 border border-gray-600 rounded-lg text-white focus:outline-none focus:ring-2 focus:ring-blue-500"
            required
          />
        </div>

        <div>
          <label className="block text-sm text-gray-400 mb-1">Hardware</label>
          <select
            value={hardware}
            onChange={(e) => setHardware(e.target.value)}
            className="w-full px-3 py-2 bg-gray-700 border border-gray-600 rounded-lg text-white focus:outline-none focus:ring-2 focus:ring-blue-500"
          >
            {HARDWARE_TYPES.map((hw) => (
              <option key={hw} value={hw}>{hw}</option>
            ))}
          </select>
        </div>

        <div>
          <label className="block text-sm text-gray-400 mb-1">Release Notes</label>
          <input
            type="text"
            value={releaseNotes}
            onChange={(e) => setReleaseNotes(e.target.value)}
            placeholder="Optional notes..."
            className="w-full px-3 py-2 bg-gray-700 border border-gray-600 rounded-lg text-white focus:outline-none focus:ring-2 focus:ring-blue-500"
          />
        </div>
      </div>

      <div className="flex items-center justify-between">
        <label className="flex items-center gap-2 text-sm text-gray-300 cursor-pointer">
          <input
            type="checkbox"
            checked={isStable}
            onChange={(e) => setIsStable(e.target.checked)}
            className="w-4 h-4 rounded bg-gray-700 border-gray-600 text-blue-600 focus:ring-blue-500"
          />
          Mark as Stable
        </label>

        <button
          type="submit"
          disabled={uploading || !file || !nodeType || !version}
          className="px-4 py-2 bg-blue-600 hover:bg-blue-700 disabled:bg-gray-600 disabled:cursor-not-allowed
                     text-white font-medium rounded-lg transition-colors flex items-center gap-2"
        >
          {uploading ? (
            <>
              <svg className="w-4 h-4 animate-spin" fill="none" viewBox="0 0 24 24">
                <circle className="opacity-25" cx="12" cy="12" r="10" stroke="currentColor" strokeWidth="4" />
                <path className="opacity-75" fill="currentColor" d="M4 12a8 8 0 018-8V0C5.373 0 0 5.373 0 12h4zm2 5.291A7.962 7.962 0 014 12H0c0 3.042 1.135 5.824 3 7.938l3-2.647z" />
              </svg>
              Uploading...
            </>
          ) : (
            <>
              <svg className="w-4 h-4" fill="none" stroke="currentColor" viewBox="0 0 24 24">
                <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2}
                  d="M4 16v1a3 3 0 003 3h10a3 3 0 003-3v-1m-4-8l-4-4m0 0L8 8m4-4v12" />
              </svg>
              Upload Firmware
            </>
          )}
        </button>
      </div>
    </form>
  );
}

export default FirmwarePanel;
