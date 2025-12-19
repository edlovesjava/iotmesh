-- Enable TimescaleDB
CREATE EXTENSION IF NOT EXISTS timescaledb;

-- Nodes table
CREATE TABLE nodes (
    id VARCHAR(10) PRIMARY KEY,       -- Mesh node ID (hex)
    name VARCHAR(50),                  -- Human-friendly name
    firmware_version VARCHAR(20),
    ip_address VARCHAR(45),            -- IPv4 or IPv6
    first_seen TIMESTAMPTZ DEFAULT NOW(),
    last_seen TIMESTAMPTZ DEFAULT NOW(),
    is_online BOOLEAN DEFAULT true,
    role VARCHAR(10) DEFAULT 'NODE',
    created_at TIMESTAMPTZ DEFAULT NOW(),
    updated_at TIMESTAMPTZ DEFAULT NOW()
);

-- Telemetry time-series
CREATE TABLE telemetry (
    time TIMESTAMPTZ NOT NULL,
    node_id VARCHAR(10) NOT NULL,
    heap_free INTEGER,
    uptime_sec INTEGER,
    peer_count INTEGER,
    role VARCHAR(10),
    CONSTRAINT telemetry_pkey PRIMARY KEY (time, node_id)
);
SELECT create_hypertable('telemetry', 'time');

-- Current state (latest values per node/key)
CREATE TABLE current_state (
    node_id VARCHAR(10) NOT NULL,
    key VARCHAR(50) NOT NULL,
    value TEXT,
    version INTEGER DEFAULT 1,
    updated_at TIMESTAMPTZ DEFAULT NOW(),
    PRIMARY KEY (node_id, key)
);

-- State history (for tracking changes over time)
CREATE TABLE state_history (
    time TIMESTAMPTZ NOT NULL,
    node_id VARCHAR(10) NOT NULL,
    key VARCHAR(50) NOT NULL,
    value TEXT,
    version INTEGER
);
SELECT create_hypertable('state_history', 'time');

-- Indexes
CREATE INDEX idx_telemetry_node_time ON telemetry (node_id, time DESC);
CREATE INDEX idx_state_history_node_key ON state_history (node_id, key, time DESC);
CREATE INDEX idx_nodes_online ON nodes (is_online);

-- Retention: 30 days telemetry, 90 days state history
SELECT add_retention_policy('telemetry', INTERVAL '30 days');
SELECT add_retention_policy('state_history', INTERVAL '90 days');
