#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
echo "Starting demo..."
# build C++ if possible
cd "$ROOT/backend_cpp"
if command -v g++ >/dev/null 2>&1; then
  g++ -std=c++17 main.cpp -I../include -lsqlite3 -pthread -O2 -o hospital_dsa_service || true
fi
# init DB
cd "$ROOT/backend"
npm install --no-audit --no-fund || true
npm run init-db || true
# start C++ service
cd "$ROOT"
if [ -f backend_cpp/hospital_dsa_service ]; then
  ./backend_cpp/hospital_dsa_service > /tmp/hms_cppsrv.log 2>&1 &
  echo "C++ DSA service started (logs: /tmp/hms_cppsrv.log)"
fi
# start node
cd "$ROOT/backend"
nohup npm start > /tmp/hms_node.log 2>&1 &
echo "Node backend started (logs: /tmp/hms_node.log)"
# serve frontend
cd "$ROOT/frontend"
if command -v python3 >/dev/null 2>&1; then
  python3 -m http.server 8000
else
  python -m SimpleHTTPServer 8000
fi
