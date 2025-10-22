#include <bits/stdc++.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sqlite3.h>
#include "../include/dsa/dsa.hpp"
using namespace std; using namespace dsa;
static AVLNode* userIndex = nullptr;
static HashMap sessionMap(1024);
static BSTNode* apptIndex = nullptr;
static MinHeap emgHeap;
string DB_PATH = "../backend/hospital.db";
static sqlite3* db = nullptr;
bool open_db(){ if(db) return true; if(sqlite3_open(DB_PATH.c_str(), &db) != SQLITE_OK){ cerr<<"sqlite open error\n"; return false; } return true; }
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
void load_from_db(){
    if(!open_db()) return;
    const char* q="SELECT user_id, username FROM users;";
    sqlite3_stmt* stmt;
    if(sqlite3_prepare_v2(db,q,-1,&stmt,nullptr)==SQLITE_OK){
        while(sqlite3_step(stmt)==SQLITE_ROW){
            int uid = sqlite3_column_int(stmt,0);
            const unsigned char* un = sqlite3_column_text(stmt,1);
            if(un){
                string username = string(reinterpret_cast<const char*>(un));
                userIndex = avl_insert(userIndex, username, uid);
            }
        }
        sqlite3_finalize(stmt);
    }
    const char* q2="SELECT appointment_id, user_id, doctor_id, time FROM appointments;";
    if(sqlite3_prepare_v2(db,q2,-1,&stmt,nullptr)==SQLITE_OK){
        while(sqlite3_step(stmt)==SQLITE_ROW){
            int aid = sqlite3_column_int(stmt,0);
            int uid = sqlite3_column_int(stmt,1);
            int did = sqlite3_column_int(stmt,2);
            const unsigned char* t = sqlite3_column_text(stmt,3);
            string time = t ? string(reinterpret_cast<const char*>(t)) : "";
            string payload = to_string(uid) + ":" + to_string(did) + ":" + time;
            apptIndex = bst_insert(apptIndex, aid, payload);
        }
        sqlite3_finalize(stmt);
    }
}
string handle_register(const vector<string>& args){
    if(args.size()<3) return join_resp({"ERR","usage: REGISTER|username|user_id"});
    string username=args[1]; int uid = stoi(args[2]);
    userIndex = avl_insert(userIndex, username, uid);
    return join_resp({"OK","inserted"});
}
string handle_session_put(const vector<string>& args){
    if(args.size()<3) return join_resp({"ERR","usage: SESSION_PUT|token|user_id"});
    string token=args[1]; int uid=stoi(args[2]);
    sessionMap.put(token, uid);
    return join_resp({"OK","session_set"});
}
string handle_session_get(const vector<string>& args){
    if(args.size()<2) return join_resp({"ERR","usage: SESSION_GET|token"});
    auto val = sessionMap.get(args[1]);
    if(val) return join_resp({"OK", to_string(*val)});
    return join_resp({"OK","-1"});
}
string handle_appt_insert(const vector<string>& args){
    if(args.size()<5) return join_resp({"ERR","usage: APPT_INSERT|aid|uid|did|time"});
    int aid = stoi(args[1]); int uid = stoi(args[2]); int did = stoi(args[3]); string time = args[4];
    string payload = to_string(uid)+":"+to_string(did)+":"+time;
    apptIndex = bst_insert(apptIndex, aid, payload);
    return join_resp({"OK","inserted"});
}
string handle_appt_list_by_user(const vector<string>& args){
    if(args.size()<2) return join_resp({"ERR","usage: APPT_LIST_BY_USER|user_id"});
    int uid = stoi(args[1]);
    vector<string> out;
    bst_inorder_collect(apptIndex, [&](BSTNode* n){
        string p = n->value;
        auto pos1 = p.find(':');
        if(pos1==string::npos) return;
        int puid = stoi(p.substr(0,pos1));
        if(puid==uid){
            auto pos2 = p.find(':', pos1+1);
            string did = p.substr(pos1+1, pos2-pos1-1);
            string time = p.substr(pos2+1);
            out.push_back(to_string(n->key)+":"+did+":"+time);
        }
    });
    string joined;
    for(size_t i=0;i<out.size();++i){ if(i) joined.push_back(','); joined += out[i]; }
    return join_resp({"OK", joined});
}
string handle_emg_push(const vector<string>& args){
    if(args.size()<3) return join_resp({"ERR","usage: EMG_PUSH|priority|name"});
    int pr = stoi(args[1]); string name=args[2];
    emgHeap.push({pr,name});
    return join_resp({"OK","pushed"});
}
string handle_emg_pop(const vector<string>& args){
    if(emgHeap.empty()) return join_resp({"OK","EMPTY"});
    auto p = emgHeap.pop();
    return join_resp({"OK", to_string(p.first), p.second});
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
    return join_resp({"ERR","unknown command"});
}
int main(){
    load_from_db();
    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);
    if((server_fd = socket(AF_INET, SOCK_STREAM, 0))==0){ perror("socket failed"); exit(EXIT_FAILURE); }
    if(setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))){ perror("setsockopt"); }
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    int port = 4001;
    if(const char* p = getenv("CPP_DSA_PORT")) port = atoi(p);
    address.sin_port = htons(port);
    if(bind(server_fd, (struct sockaddr*)&address, sizeof(address))<0){ perror("bind failed"); exit(EXIT_FAILURE); }
    if(listen(server_fd, 10)<0){ perror("listen"); exit(EXIT_FAILURE); }
    printf("C++ DSA service listening on 0.0.0.0:%d\n", port);
    fflush(stdout);
    while(true){
        if((new_socket = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen))<0){ perror("accept"); continue; }
        string line;
        char buf[1024];
        ssize_t n;
        while((n = read(new_socket, buf, sizeof(buf)))>0){
            for(ssize_t i=0;i<n;i++){
                char c = buf[i];
                if(c=='\n'){
                    string resp = process_line(line);
                    resp.push_back('\n');
                    write(new_socket, resp.c_str(), resp.size());
                    line.clear();
                } else {
                    line.push_back(c);
                }
            }
        }
        close(new_socket);
    }
    return 0;
}
