#!/bin/bash

# EAS Build Pre-Install Hook
# This script runs before npm install

set -e

echo "Running pre-install hook..."

# Ensure node is in PATH
which node || echo "Node not found in PATH"
node --version

# Print environment info
echo "Build environment ready"
