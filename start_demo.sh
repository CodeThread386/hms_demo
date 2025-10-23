#!/usr/bin/env bash
set -e

ROOT="$(cd "$(dirname "$0")" && pwd)"
echo "🏥 Starting Hospital Management System..."

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Function to check if command exists
command_exists() {
    command -v "$1" >/dev/null 2>&1
}

# Function to kill processes on port
kill_port() {
    local port=$1
    echo "Checking port $port..."
    
    # Windows (Git Bash/MinGW)
    if [[ "$OSTYPE" == "msys" || "$OSTYPE" == "win32" ]]; then
        # Use netstat to find PID and taskkill to stop it
        for pid in $(netstat -ano | grep ":$port" | awk '{print $5}' | sort -u); do
            if [[ "$pid" =~ ^[0-9]+$ ]]; then
                echo "Killing process $pid on port $port"
                taskkill //F //PID $pid 2>/dev/null || true
            fi
        done
    # Linux/Mac
    elif command_exists lsof; then
        lsof -ti:$port | xargs kill -9 2>/dev/null || true
    elif command_exists fuser; then
        fuser -k $port/tcp 2>/dev/null || true
    fi
    
    sleep 1
}

# Cleanup function
cleanup() {
    echo -e "\n${YELLOW}Shutting down services...${NC}"
    kill_port 4001
    kill_port 4000
    kill_port 8000
    exit 0
}

trap cleanup SIGINT SIGTERM

# Step 1: Build C++ DSA Service
echo -e "${GREEN}Step 1: Building C++ DSA Service...${NC}"
cd "$ROOT/backend_cpp"

if command_exists g++; then
    echo "Compiling with g++..."
    if [[ "$OSTYPE" == "msys" || "$OSTYPE" == "win32" ]]; then
        # Windows/MinGW compilation
        g++ -std=c++17 main.cpp -lws2_32 -O2 -o hospital_dsa_service.exe 2>&1 || {
            echo -e "${RED}C++ compilation failed. Service will run without DSA optimization.${NC}"
        }
    else
        # Linux/Mac compilation
        g++ -std=c++17 main.cpp -pthread -O2 -o hospital_dsa_service 2>&1 || {
            echo -e "${RED}C++ compilation failed. Service will run without DSA optimization.${NC}"
        }
    fi
elif command_exists clang++; then
    echo "Compiling with clang++..."
    clang++ -std=c++17 main.cpp -pthread -O2 -o hospital_dsa_service 2>&1 || {
        echo -e "${RED}C++ compilation failed. Service will run without DSA optimization.${NC}"
    }
else
    echo -e "${YELLOW}No C++ compiler found. Skipping C++ service.${NC}"
fi

# Step 2: Setup Node.js Backend
echo -e "${GREEN}Step 2: Setting up Node.js backend...${NC}"
cd "$ROOT/backend"

if ! command_exists node; then
    echo -e "${RED}Node.js is not installed. Please install Node.js first.${NC}"
    exit 1
fi

if ! command_exists npm; then
    echo -e "${RED}npm is not installed. Please install npm first.${NC}"
    exit 1
fi

echo "Installing dependencies..."
npm install 2>&1 || {
    echo -e "${RED}npm install failed${NC}"
    exit 1
}

echo "Initializing database..."
npm run init-db 2>&1 || {
    echo -e "${RED}Database initialization failed${NC}"
    exit 1
}

# Step 3: Start C++ DSA Service
echo -e "${GREEN}Step 3: Starting C++ DSA Service...${NC}"
if [ -f "$ROOT/backend_cpp/hospital_dsa_service" ]; then
    kill_port 4001
    "$ROOT/backend_cpp/hospital_dsa_service" > /tmp/hms_cppsrv.log 2>&1 &
    CPP_PID=$!
    sleep 2
    if ps -p $CPP_PID > /dev/null; then
        echo -e "${GREEN}✓ C++ DSA service started on port 4001 (PID: $CPP_PID)${NC}"
        echo "  Logs: /tmp/hms_cppsrv.log"
    else
        echo -e "${YELLOW}⚠ C++ service failed to start, continuing without it${NC}"
    fi
else
    echo -e "${YELLOW}⚠ C++ service not compiled, continuing without it${NC}"
fi

# Step 4: Start Node.js Backend
echo -e "${GREEN}Step 4: Starting Node.js backend...${NC}"
cd "$ROOT/backend"
kill_port 4000
npm start > /tmp/hms_node.log 2>&1 &
NODE_PID=$!
sleep 3

if ps -p $NODE_PID > /dev/null; then
    echo -e "${GREEN}✓ Node.js backend started on port 4000 (PID: $NODE_PID)${NC}"
    echo "  Logs: /tmp/hms_node.log"
else
    echo -e "${RED}✗ Node.js backend failed to start${NC}"
    cat /tmp/hms_node.log
    cleanup
    exit 1
fi

# Step 5: Start Frontend Server
echo -e "${GREEN}Step 5: Starting frontend server...${NC}"
cd "$ROOT/frontend"

if [ ! -f "index.html" ]; then
    echo -e "${RED}Frontend files not found!${NC}"
    cleanup
    exit 1
fi

kill_port 8000

if command_exists python3; then
    echo "Using Python 3..."
    python3 -m http.server 8000 > /tmp/hms_frontend.log 2>&1 &
elif command_exists python; then
    echo "Using Python 2..."
    python -m SimpleHTTPServer 8000 > /tmp/hms_frontend.log 2>&1 &
else
    echo -e "${RED}Python not found. Cannot start frontend server.${NC}"
    cleanup
    exit 1
fi

FRONTEND_PID=$!
sleep 2

if ps -p $FRONTEND_PID > /dev/null; then
    echo -e "${GREEN}✓ Frontend server started on port 8000 (PID: $FRONTEND_PID)${NC}"
    echo "  Logs: /tmp/hms_frontend.log"
else
    echo -e "${RED}✗ Frontend server failed to start${NC}"
    cleanup
    exit 1
fi

# Success message
echo -e "\n${GREEN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${GREEN}✓ Hospital Management System is running!${NC}"
echo -e "${GREEN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "\n📱 Access the application at:"
echo -e "   ${GREEN}http://localhost:8000${NC}"
echo -e "\n🔧 API Backend:"
echo -e "   ${GREEN}http://localhost:4000${NC}"
echo -e "\n📊 Services Status:"
echo -e "   Frontend:  http://localhost:8000"
echo -e "   Backend:   http://localhost:4000"
echo -e "   C++ DSA:   localhost:4001"
echo -e "\n📝 Logs:"
echo -e "   Frontend:  /tmp/hms_frontend.log"
echo -e "   Backend:   /tmp/hms_node.log"
echo -e "   C++ DSA:   /tmp/hms_cppsrv.log"
echo -e "\n${YELLOW}Press Ctrl+C to stop all services${NC}\n"

# Keep script running and wait for interrupt
wait