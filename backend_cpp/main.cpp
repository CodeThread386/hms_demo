// server_win.cpp
// Single-file Windows-compatible replacement for your original program.
// Uses Winsock2 and simple file-based persistence. No external libs required
// (except linking Ws2_32 which is part of Windows SDK).

#define _CRT_SECURE_NO_WARNINGS
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <map>
#include <queue>
#include <mutex>
#include <fstream>
#include <sstream>
#include <atomic>
#include <csignal>

#pragma comment(lib, "Ws2_32.lib")

using namespace std;

// ---------- Simple data storage (in-memory) ----------
struct AppState {
    unordered_map<string,int> userIndex; // username -> user_id
    map<int,string> apptIndex;           // appointment_id -> payload
    map<int,string> historyIndex;        // record_id -> payload
    unordered_map<string,int> sessionMap;// token -> uid

    // Emergency heap: min-heap on priority (lower number = higher priority)
    struct EMGItem { int pr; string name; };
    struct Cmp { bool operator()(const EMGItem& a, const EMGItem& b) const { return a.pr > b.pr; } };
    priority_queue<EMGItem, vector<EMGItem>, Cmp> emgHeap;

    mutex mtx;

    // persistence file paths
    string users_file = "users.txt";
    string appts_file = "appointments.txt";
    string history_file = "history.txt";

    AppState() {}
};

// global state
static AppState app;
static SOCKET listenSock = INVALID_SOCKET;
static atomic<bool> keep_running(true);

// ---------- Helper functions for escaping/fields ----------
vector<string> split_fields(const string& line){
    vector<string> out; string cur; bool escape=false;
    for(char c: line){
        if(escape){ cur.push_back(c); escape=false; continue; }
        if(c=='\\'){ escape=true; continue; }
        if(c=='|'){ out.push_back(cur); cur.clear(); continue; }
        cur.push_back(c);
    }
    out.push_back(cur); return out;
}
string join_resp(const vector<string>& parts){
    string out;
    for(size_t i=0;i<parts.size();++i){
        if(i) out.push_back('|');
        for(char c: parts[i]){ if(c=='|'||c=='\\') out.push_back('\\'); out.push_back(c); }
    }
    return out;
}

// ---------- Persistence (simple) ----------
void load_from_files() {
    lock_guard<mutex> lk(app.mtx);
    // users
    ifstream us(app.users_file);
    if(us){
        string line;
        while(getline(us,line)){
            if(line.empty()) continue;
            auto f = split_fields(line);
            // format: user_id|username
            if(f.size()>=2){
                int uid = stoi(f[0]);
                string uname = f[1];
                app.userIndex[uname] = uid;
            }
        }
    }
    // appointments
    ifstream ap(app.appts_file);
    if(ap){
        string line;
        while(getline(ap,line)){
            if(line.empty()) continue;
            auto f = split_fields(line);
            // format: appointment_id|payload
            if(f.size()>=2){
                int aid = stoi(f[0]);
                string payload = f[1];
                app.apptIndex[aid] = payload;
            }
        }
    }
    // history
    ifstream hs(app.history_file);
    if(hs){
        string line;
        while(getline(hs,line)){
            if(line.empty()) continue;
            auto f = split_fields(line);
            // format: record_id|payload
            if(f.size()>=2){
                int rid = stoi(f[0]);
                string payload = f[1];
                app.historyIndex[rid] = payload;
            }
        }
    }
}
void append_user_to_file(int uid, const string& username){
    lock_guard<mutex> lk(app.mtx);
    ofstream os(app.users_file, ios::app);
    if(!os) return;
    os << join_resp({ to_string(uid), username }) << "\n";
}
void append_appt_to_file(int aid, const string& payload){
    lock_guard<mutex> lk(app.mtx);
    ofstream os(app.appts_file, ios::app);
    if(!os) return;
    os << join_resp({ to_string(aid), payload }) << "\n";
}
void append_history_to_file(int rid, const string& payload){
    lock_guard<mutex> lk(app.mtx);
    ofstream os(app.history_file, ios::app);
    if(!os) return;
    os << join_resp({ to_string(rid), payload }) << "\n";
}

// ---------- Command handlers ----------
string handle_register(const vector<string>& args) {
    try {
        if(args.size() < 3) return join_resp({"ERR", "Invalid arguments"});
        const string& username = args[1];
        int uid = stoi(args[2]);

        lock_guard<mutex> lk(app.mtx);
        if(app.userIndex.find(username) != app.userIndex.end()) {
            return join_resp({"ERR", "Username already exists"});
        }
        app.userIndex[username] = uid;
        append_user_to_file(uid, username);
        return join_resp({"OK", "User registered successfully"});
    } catch(const exception& e) {
        return join_resp({"ERR", string("Error: ") + e.what()});
    }
}

string handle_session_put(const vector<string>& args){
    if(args.size()<3) return join_resp({"ERR","usage: SESSION_PUT|token|user_id"});
    string token=args[1]; int uid=stoi(args[2]);
    lock_guard<mutex> lk(app.mtx);
    app.sessionMap[token] = uid;
    return join_resp({"OK","session_set"});
}
string handle_session_get(const vector<string>& args){
    if(args.size()<2) return join_resp({"ERR","usage: SESSION_GET|token"});
    lock_guard<mutex> lk(app.mtx);
    auto it = app.sessionMap.find(args[1]);
    if(it!=app.sessionMap.end()) return join_resp({"OK", to_string(it->second)});
    return join_resp({"OK","-1"});
}

string handle_appt_insert(const vector<string>& args) {
    try {
        if(args.size() < 5) return join_resp({"ERR", "Invalid arguments"});
        int aid = stoi(args[1]);
        int uid = stoi(args[2]);
        int did = stoi(args[3]);
        const string& time = args[4];
        if(time.empty()) return join_resp({"ERR", "Invalid time"});

        lock_guard<mutex> lk(app.mtx);
        // check conflicts by searching any payload that contains same time
        for(const auto& kv : app.apptIndex){
            if(kv.second.find(time) != string::npos){
                return join_resp({"ERR", "Time slot already booked"});
            }
        }

        string payload = to_string(uid) + ":" + to_string(did) + ":" + time;
        app.apptIndex[aid] = payload;
        append_appt_to_file(aid, payload);
        return join_resp({"OK", "Appointment booked successfully"});
    } catch(const exception& e) {
        return join_resp({"ERR", string("Error: ") + e.what()});
    }
}

string handle_appt_list_by_user(const vector<string>& args){
    if(args.size()<2) return join_resp({"ERR","usage: APPT_LIST_BY_USER|user_id"});
    int uid = stoi(args[1]);
    vector<string> out;
    lock_guard<mutex> lk(app.mtx);
    for(const auto& kv : app.apptIndex){
        string p = kv.second;
        auto pos1 = p.find(':');
        if(pos1==string::npos) continue;
        int puid = stoi(p.substr(0,pos1));
        if(puid==uid){
            auto pos2 = p.find(':', pos1+1);
            string did = pos2==string::npos ? "" : p.substr(pos1+1, pos2-pos1-1);
            string time = pos2==string::npos ? "" : p.substr(pos2+1);
            out.push_back(to_string(kv.first)+":"+did+":"+time);
        }
    }
    string joined;
    for(size_t i=0;i<out.size();++i){ if(i) joined.push_back(','); joined += out[i]; }
    return join_resp({"OK", joined});
}

string handle_emg_push(const vector<string>& args){
    if(args.size()<3) return join_resp({"ERR","usage: EMG_PUSH|priority|name"});
    int pr = stoi(args[1]); string name=args[2];
    lock_guard<mutex> lk(app.mtx);
    app.emgHeap.push({pr,name});
    return join_resp({"OK","pushed"});
}
string handle_emg_pop(const vector<string>& args){
    lock_guard<mutex> lk(app.mtx);
    if(app.emgHeap.empty()) return join_resp({"OK","EMPTY"});
    auto p = app.emgHeap.top(); app.emgHeap.pop();
    return join_resp({"OK", to_string(p.pr), p.name});
}
string handle_emg_size(const vector<string>& args){
    lock_guard<mutex> lk(app.mtx);
    return join_resp({"OK", to_string((int)app.emgHeap.size())});
}

string handle_history_insert(const vector<string>& args){
    if(args.size()<3) return join_resp({"ERR","usage: HISTORY_INSERT|record_id|payload"});
    int rid = stoi(args[1]); string payload = args[2];
    {
        lock_guard<mutex> lk(app.mtx);
        app.historyIndex[rid] = payload;
    }
    append_history_to_file(rid, payload);
    return join_resp({"OK","inserted"});
}

string handle_doctor_search(const vector<string>& args){
    if(args.size()<2) return join_resp({"ERR","usage: DOCTOR_SEARCH|name"});
    string name = args[1];
    lock_guard<mutex> lk(app.mtx);
    auto it = app.userIndex.find(name);
    if(it!=app.userIndex.end()) return join_resp({"OK", to_string(it->second)});
    return join_resp({"OK","NOT_FOUND"});
}

string process_line(const string& line){
    auto fields = split_fields(line);
    if(fields.empty()) return join_resp({"ERR","empty"});
    string cmd = fields[0];
    if(cmd=="REGISTER") return handle_register(fields);
    if(cmd=="SESSION_PUT") return handle_session_put(fields);
    if(cmd=="SESSION_GET") return handle_session_get(fields);
    if(cmd=="APPT_INSERT") return handle_appt_insert(fields);
    if(cmd=="APPT_LIST_BY_USER") return handle_appt_list_by_user(fields);
    if(cmd=="EMG_PUSH") return handle_emg_push(fields);
    if(cmd=="EMG_POP") return handle_emg_pop(fields);
    if(cmd=="EMG_SIZE") return handle_emg_size(fields);
    if(cmd=="HISTORY_INSERT") return handle_history_insert(fields);
    if(cmd=="DOCTOR_SEARCH") return handle_doctor_search(fields);
    return join_resp({"ERR","unknown command"});
}

// ---------- Cleanup and Console handler ----------
BOOL WINAPI ConsoleHandlerRoutine(DWORD dwCtrlType) {
    switch(dwCtrlType) {
        case CTRL_C_EVENT:
        case CTRL_CLOSE_EVENT:
        case CTRL_BREAK_EVENT:
        case CTRL_SHUTDOWN_EVENT:
        {
            keep_running = false;
            // close listen socket to break accept()
            if(listenSock != INVALID_SOCKET){
                closesocket(listenSock);
                listenSock = INVALID_SOCKET;
            }
            // allow some time (main loop will exit)
            Sleep(200);
            return TRUE;
        }
        default:
            return FALSE;
    }
}

// ---------- Networking (Winsock) ----------
int run_server(int port) {
    WSADATA wsaData;
    int rv = WSAStartup(MAKEWORD(2,2), &wsaData);
    if(rv != 0){
        cerr << "WSAStartup failed: " << rv << "\n";
        return 1;
    }

    addrinfo hints{}, *res = nullptr;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    string portstr = to_string(port);
    if(getaddrinfo(nullptr, portstr.c_str(), &hints, &res) != 0){
        cerr << "getaddrinfo failed\n";
        WSACleanup();
        return 2;
    }

    listenSock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if(listenSock == INVALID_SOCKET){
        cerr << "socket failed\n";
        freeaddrinfo(res);
        WSACleanup();
        return 3;
    }

    // allow reuse (best-effort)
    char opt = 1;
    setsockopt(listenSock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    if(bind(listenSock, res->ai_addr, (int)res->ai_addrlen) == SOCKET_ERROR){
        cerr << "bind failed\n";
        freeaddrinfo(res);
        closesocket(listenSock);
        WSACleanup();
        return 4;
    }
    freeaddrinfo(res);

    if(listen(listenSock, SOMAXCONN) == SOCKET_ERROR){
        cerr << "listen failed\n";
        closesocket(listenSock);
        WSACleanup();
        return 5;
    }

    cout << "C++ service listening on 0.0.0.0:" << port << "\n";
    fflush(stdout);

    while(keep_running){
        sockaddr_in clientAddr; int addrlen = sizeof(clientAddr);
        SOCKET clientSock = accept(listenSock, (sockaddr*)&clientAddr, &addrlen);
        if(clientSock == INVALID_SOCKET){
            if(!keep_running) break;
            cerr << "accept failed\n";
            continue;
        }

        // Simple single-threaded handling (reads until connection close)
        string line;
        char buf[1024];
        int n;
        while((n = recv(clientSock, buf, sizeof(buf), 0)) > 0){
            for(int i=0;i<n;i++){
                char c = buf[i];
                if(c=='\n'){
                    string resp = process_line(line);
                    resp.push_back('\n');
                    send(clientSock, resp.c_str(), (int)resp.size(), 0);
                    line.clear();
                } else if(c!='\r'){
                    line.push_back(c);
                }
            }
        }
        closesocket(clientSock);
    }

    // cleanup
    if(listenSock != INVALID_SOCKET) { closesocket(listenSock); listenSock = INVALID_SOCKET; }
    WSACleanup();
    return 0;
}

int main(int argc, char** argv){
    // Register console handler
    SetConsoleCtrlHandler(ConsoleHandlerRoutine, TRUE);

    // Load persisted files
    load_from_files();

    int port = 4001;
    const char* env = getenv("CPP_DSA_PORT");
    if(env) port = atoi(env);

    int rc = run_server(port);

    cout << "Server exiting with code " << rc << "\n";
    return rc;
}
