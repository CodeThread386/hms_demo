Hospital Management System — Demo (Node backend using C++ DSA service)
Overview:
- Node backend (backend/) is the API server that the frontend talks to (http://localhost:4000).
- A C++ DSA service (backend_cpp/) runs on port 4001 and maintains in-memory DSA structures (AVL, HashMap, MinHeap, BST).
- Node delegates DSA operations to the C++ service by TCP connection on port 4001.
How to run:
1) Build C++ service: cd backend_cpp && g++ -std=c++17 main.cpp -I../include -lsqlite3 -pthread -O2 -o hospital_dsa_service && ./hospital_dsa_service &
2) Node backend: cd backend && npm install && npm run init-db && npm start &
3) Frontend: cd frontend && python3 -m http.server 8000
Open http://localhost:8000
