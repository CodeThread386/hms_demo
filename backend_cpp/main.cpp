#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <map>
#include <queue>
#include <sstream>
#include <csignal>
#include <cstring>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "Ws2_32.lib")
    typedef SOCKET socket_t;
    #define CLOSE_SOCKET closesocket
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <unistd.h>
    #include <arpa/inet.h>
    typedef int socket_t;
    #define INVALID_SOCKET -1
    #define SOCKET_ERROR -1
    #define CLOSE_SOCKET close
#endif

using namespace std;

// Data structures - Single-threaded version for Windows compatibility
struct AppState {
    unordered_map<string, int> userIndex;
    map<int, string> apptIndex;
    map<int, string> historyIndex;
    unordered_map<string, int> sessionMap;

    struct EMGItem {
        int pr;
        string name;
        bool operator>(const EMGItem& other) const {
            return pr > other.pr;
        }
    };
    priority_queue<EMGItem, vector<EMGItem>, greater<EMGItem>> emgHeap;
};

static AppState app;
static volatile bool keep_running = true;

void signal_handler(int signal) {
    keep_running = false;
}

vector<string> split_fields(const string& line) {
    vector<string> out;
    string cur;
    bool escape = false;
    for (char c : line) {
        if (escape) {
            cur.push_back(c);
            escape = false;
            continue;
        }
        if (c == '\\') {
            escape = true;
            continue;
        }
        if (c == '|') {
            out.push_back(cur);
            cur.clear();
            continue;
        }
        cur.push_back(c);
    }
    out.push_back(cur);
    return out;
}

string join_resp(const vector<string>& parts) {
    string out;
    for (size_t i = 0; i < parts.size(); ++i) {
        if (i) out.push_back('|');
        for (char c : parts[i]) {
            if (c == '|' || c == '\\') out.push_back('\\');
            out.push_back(c);
        }
    }
    return out;
}

string handle_register(const vector<string>& args) {
    try {
        if (args.size() < 3) return join_resp({"ERR", "Invalid arguments"});
        const string& username = args[1];
        int uid = stoi(args[2]);

        if (app.userIndex.find(username) != app.userIndex.end()) {
            return join_resp({"ERR", "Username already exists"});
        }
        app.userIndex[username] = uid;
        return join_resp({"OK", "User registered successfully"});
    } catch (const exception& e) {
        return join_resp({"ERR", string("Error: ") + e.what()});
    }
}

string handle_session_put(const vector<string>& args) {
    if (args.size() < 3) return join_resp({"ERR", "usage: SESSION_PUT|token|user_id"});
    string token = args[1];
    int uid = stoi(args[2]);
    app.sessionMap[token] = uid;
    return join_resp({"OK", "session_set"});
}

string handle_session_get(const vector<string>& args) {
    if (args.size() < 2) return join_resp({"ERR", "usage: SESSION_GET|token"});
    auto it = app.sessionMap.find(args[1]);
    if (it != app.sessionMap.end()) return join_resp({"OK", to_string(it->second)});
    return join_resp({"OK", "-1"});
}

string handle_appt_insert(const vector<string>& args) {
    try {
        if (args.size() < 5) return join_resp({"ERR", "Invalid arguments"});
        int aid = stoi(args[1]);
        int uid = stoi(args[2]);
        int did = stoi(args[3]);
        const string& time = args[4];

        string payload = to_string(uid) + ":" + to_string(did) + ":" + time;
        app.apptIndex[aid] = payload;
        return join_resp({"OK", "Appointment booked successfully"});
    } catch (const exception& e) {
        return join_resp({"ERR", string("Error: ") + e.what()});
    }
}

string handle_appt_list_by_user(const vector<string>& args) {
    if (args.size() < 2) return join_resp({"ERR", "usage: APPT_LIST_BY_USER|user_id"});
    int uid = stoi(args[1]);
    vector<string> out;
    
    for (const auto& kv : app.apptIndex) {
        string p = kv.second;
        auto pos1 = p.find(':');
        if (pos1 == string::npos) continue;
        int puid = stoi(p.substr(0, pos1));
        if (puid == uid) {
            auto pos2 = p.find(':', pos1 + 1);
            string did = pos2 == string::npos ? "" : p.substr(pos1 + 1, pos2 - pos1 - 1);
            string time = pos2 == string::npos ? "" : p.substr(pos2 + 1);
            out.push_back(to_string(kv.first) + ":" + did + ":" + time);
        }
    }
    string joined;
    for (size_t i = 0; i < out.size(); ++i) {
        if (i) joined.push_back(',');
        joined += out[i];
    }
    return join_resp({"OK", joined});
}

string handle_emg_push(const vector<string>& args) {
    if (args.size() < 3) return join_resp({"ERR", "usage: EMG_PUSH|priority|name"});
    int pr = stoi(args[1]);
    string name = args[2];
    app.emgHeap.push({pr, name});
    return join_resp({"OK", "pushed"});
}

string handle_emg_pop(const vector<string>& args) {
    if (app.emgHeap.empty()) return join_resp({"OK", "EMPTY"});
    auto p = app.emgHeap.top();
    app.emgHeap.pop();
    return join_resp({"OK", to_string(p.pr), p.name});
}

string handle_emg_size(const vector<string>& args) {
    return join_resp({"OK", to_string((int)app.emgHeap.size())});
}

string handle_history_insert(const vector<string>& args) {
    if (args.size() < 3) return join_resp({"ERR", "usage: HISTORY_INSERT|record_id|payload"});
    int rid = stoi(args[1]);
    string payload = args[2];
    app.historyIndex[rid] = payload;
    return join_resp({"OK", "inserted"});
}

string handle_doctor_search(const vector<string>& args) {
    if (args.size() < 2) return join_resp({"ERR", "usage: DOCTOR_SEARCH|name"});
    string name = args[1];
    auto it = app.userIndex.find(name);
    if (it != app.userIndex.end()) return join_resp({"OK", to_string(it->second)});
    return join_resp({"OK", "NOT_FOUND"});
}

string process_line(const string& line) {
    auto fields = split_fields(line);
    if (fields.empty()) return join_resp({"ERR", "empty"});
    string cmd = fields[0];
    if (cmd == "REGISTER") return handle_register(fields);
    if (cmd == "SESSION_PUT") return handle_session_put(fields);
    if (cmd == "SESSION_GET") return handle_session_get(fields);
    if (cmd == "APPT_INSERT") return handle_appt_insert(fields);
    if (cmd == "APPT_LIST_BY_USER") return handle_appt_list_by_user(fields);
    if (cmd == "EMG_PUSH") return handle_emg_push(fields);
    if (cmd == "EMG_POP") return handle_emg_pop(fields);
    if (cmd == "EMG_SIZE") return handle_emg_size(fields);
    if (cmd == "HISTORY_INSERT") return handle_history_insert(fields);
    if (cmd == "DOCTOR_SEARCH") return handle_doctor_search(fields);
    return join_resp({"ERR", "unknown command"});
}

void handle_client(socket_t client_sock) {
    string line;
    char buf[1024];
    int n;
    
    while ((n = recv(client_sock, buf, sizeof(buf), 0)) > 0) {
        for (int i = 0; i < n; i++) {
            char c = buf[i];
            if (c == '\n') {
                string resp = process_line(line);
                resp.push_back('\n');
                send(client_sock, resp.c_str(), (int)resp.size(), 0);
                line.clear();
            } else if (c != '\r') {
                line.push_back(c);
            }
        }
    }
    
    CLOSE_SOCKET(client_sock);
}

int main(int argc, char** argv) {
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    int port = 4001;
    const char* env = getenv("CPP_DSA_PORT");
    if (env) port = atoi(env);

#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        cerr << "WSAStartup failed\n";
        return 1;
    }
#endif

    socket_t listen_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_sock == INVALID_SOCKET) {
        cerr << "Socket creation failed\n";
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }

    int opt = 1;
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));

    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(listen_sock, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        cerr << "Bind failed\n";
        CLOSE_SOCKET(listen_sock);
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }

    if (listen(listen_sock, 10) == SOCKET_ERROR) {
        cerr << "Listen failed\n";
        CLOSE_SOCKET(listen_sock);
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }

    cout << "C++ DSA service listening on 0.0.0.0:" << port << endl;

    while (keep_running) {
        sockaddr_in client_addr;
#ifdef _WIN32
        int client_len = sizeof(client_addr);
#else
        socklen_t client_len = sizeof(client_addr);
#endif
        socket_t client_sock = accept(listen_sock, (sockaddr*)&client_addr, &client_len);
        
        if (client_sock == INVALID_SOCKET) {
            if (!keep_running) break;
            continue;
        }

        // Handle client synchronously (single-threaded for Windows MinGW compatibility)
        handle_client(client_sock);
    }

    CLOSE_SOCKET(listen_sock);
#ifdef _WIN32
    WSACleanup();
#endif

    cout << "Server shutting down" << endl;
    return 0;
}