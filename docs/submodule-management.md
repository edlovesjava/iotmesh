# MeshSwarm Submodule Management

MeshSwarm is included as a git submodule in iotmesh. This guide covers common submodule operations.

## Initial Clone

Clone iotmesh with the submodule included:

```bash
git clone --recurse-submodules https://github.com/edlovesjava/iotmesh.git
```

Or if you already cloned without submodules:

```bash
git submodule update --init --recursive
```

## Update to Latest

Pull the latest MeshSwarm changes from upstream:

```bash
# Update submodule to latest commit on its tracked branch
git submodule update --remote MeshSwarm

# Commit the submodule pointer update
git add MeshSwarm
git commit -m "Update MeshSwarm submodule to latest"
```

## Local Development

To make changes to MeshSwarm while working on iotmesh:

```bash
# Enter the submodule directory
cd MeshSwarm

# Create a branch for your changes
git checkout -b my-feature

# Make changes, commit them
git add .
git commit -m "My changes"

# Push to MeshSwarm repo (if you have access)
git push origin my-feature

# Return to iotmesh root
cd ..

# Update iotmesh to point to your new commit
git add MeshSwarm
git commit -m "Update MeshSwarm to my-feature branch"
```

## Check Status

```bash
# See which commit the submodule is on
git submodule status

# See if submodule has local changes
cd MeshSwarm && git status
```

## Common Issues

### Submodule Not Initialized

Shows `-` prefix in `git submodule status`:

```bash
git submodule update --init
```

### Detached HEAD in Submodule

This is normal after `git submodule update`. To work on a branch:

```bash
cd MeshSwarm
git checkout main  # or your working branch
```

### PlatformIO Can't Find Library

Ensure the submodule is initialized. The `firmware/platformio.ini` uses `symlink://../MeshSwarm` which requires the submodule directory to exist.

```bash
# Verify submodule exists
ls MeshSwarm/library.properties

# If missing, initialize it
git submodule update --init
```

### Submodule Has Uncommitted Changes

If `git status` shows the submodule as modified but you haven't made changes:

```bash
# Reset submodule to expected commit
git submodule update --force MeshSwarm
```

## Switching Between Submodule and External Repo

For development, you may want to use a separate MeshSwarm clone instead of the submodule.

### Use External Repo

Edit `firmware/platformio.ini`:

```ini
lib_deps =
    ; Use external repo (sibling directory)
    symlink://../../MeshSwarm
```

### Use Submodule (default)

```ini
lib_deps =
    ; Use submodule
    symlink://../MeshSwarm
```

### Use GitHub Directly

```ini
lib_deps =
    ; Use GitHub (no local changes)
    https://github.com/edlovesjava/MeshSwarm.git
```
