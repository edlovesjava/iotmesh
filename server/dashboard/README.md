# IoT Mesh Dashboard

React-based web dashboard for monitoring the mesh network in real-time.

## Features

- **Node Grid**: Visual status of all mesh nodes (online/offline)
- **State Table**: Live view of shared state across the network
- **Auto-refresh**: Updates every 5 seconds
- **Responsive**: Works on desktop and mobile

## Quick Start

```bash
# Install dependencies
npm install

# Start development server (with API proxy to port 8000)
npm run dev

# Build for production
npm run build
```

## Development

The dashboard expects the API server running on `http://localhost:8000`. The Vite dev server proxies `/api` requests automatically.

```bash
# Start the API server first
cd ../
docker-compose up

# Then start the dashboard
cd dashboard
npm run dev
```

Open http://localhost:5173 in your browser.

## Tech Stack

- React 18
- Vite
- TailwindCSS v4
- Fetch API for data loading

## Project Structure

```
src/
├── api/
│   └── mesh.js        # API client functions
├── components/
│   ├── NodeGrid.jsx   # Node status grid
│   └── StateTable.jsx # State table component
├── utils/
│   └── time.js        # Time formatting utilities
├── App.jsx            # Main application
├── main.jsx           # Entry point
└── index.css          # TailwindCSS imports
```

## API Endpoints Used

| Endpoint | Description |
|----------|-------------|
| `GET /api/v1/nodes` | List all registered nodes |
| `GET /api/v1/state` | Get current shared state |

## Production Build

```bash
npm run build
```

Output is in `dist/` folder. Serve with any static file server or nginx.
