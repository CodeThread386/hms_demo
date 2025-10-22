#pragma once
#include <vector>
#include <string>
#include <functional>
#include <utility>
#include <optional>
#include <algorithm>
#include <mutex>
namespace dsa {
struct AVLNode { std::string key; int value; AVLNode* left=nullptr; AVLNode* right=nullptr; int height=1; AVLNode(const std::string& k,int v):key(k),value(v){} };
inline int height(AVLNode* n){ return n? n->height:0; }
inline void update_height(AVLNode* n){ if(n) n->height = 1 + std::max(height(n->left), height(n->right)); }
inline int balance_factor(AVLNode* n){ return n? height(n->left)-height(n->right):0; }
AVLNode* rotate_right(AVLNode* y){ AVLNode* x=y->left; AVLNode* T2=x->right; x->right=y; y->left=T2; update_height(y); update_height(x); return x; }
AVLNode* rotate_left(AVLNode* x){ AVLNode* y=x->right; AVLNode* T2=y->left; y->left=x; x->right=T2; update_height(x); update_height(y); return y; }
AVLNode* avl_insert(AVLNode* node, const std::string& key, int value){ if(!node) return new AVLNode(key,value); if(key<node->key) node->left=avl_insert(node->left,key,value); else if(key>node->key) node->right=avl_insert(node->right,key,value); else { node->value=value; return node;} update_height(node); int bf=balance_factor(node); if(bf>1 && key < node->left->key) return rotate_right(node); if(bf< -1 && key > node->right->key) return rotate_left(node); if(bf>1 && key > node->left->key){ node->left=rotate_left(node->left); return rotate_right(node);} if(bf< -1 && key < node->right->key){ node->right=rotate_right(node->right); return rotate_left(node);} return node; }
std::optional<int> avl_find(AVLNode* node, const std::string& key){ while(node){ if(key==node->key) return node->value; node = key < node->key ? node->left : node->right; } return std::nullopt; }
struct HashMap { size_t capacity=512; std::vector<std::vector<std::pair<std::string,int>>> table; std::mutex m; HashMap(size_t cap=512):capacity(cap),table(cap){} size_t h(const std::string& s) const{ size_t hash=1469598103934665603u; for(char c:s) hash=(hash ^ (unsigned char)c)*1099511628211u; return hash%capacity; } void put(const std::string& k,int v){ std::lock_guard<std::mutex> lk(m); auto &bucket=table[h(k)]; for(auto &p:bucket) if(p.first==k){ p.second=v; return;} bucket.emplace_back(k,v);} std::optional<int> get(const std::string& k){ std::lock_guard<std::mutex> lk(m); auto &bucket=table[h(k)]; for(auto &p:bucket) if(p.first==k) return p.second; return std::nullopt; } void remove(const std::string& k){ std::lock_guard<std::mutex> lk(m); auto &bucket=table[h(k)]; bucket.erase(std::remove_if(bucket.begin(), bucket.end(), [&](auto &p){ return p.first==k;}), bucket.end()); } };
struct MinHeap { std::vector<std::pair<int,std::string>> a; MinHeap(){} void push(const std::pair<int,std::string>& p){ a.push_back(p); std::push_heap(a.begin(), a.end(), std::greater<>()); } bool empty() const { return a.empty(); } std::pair<int,std::string> pop(){ std::pop_heap(a.begin(), a.end(), std::greater<>()); auto t=a.back(); a.pop_back(); return t; } size_t size() const { return a.size(); } };
struct BSTNode { int key; std::string value; BSTNode* left=nullptr; BSTNode* right=nullptr; BSTNode(int k,const std::string& v):key(k),value(v){} };
BSTNode* bst_insert(BSTNode* node,int key,const std::string& value){ if(!node) return new BSTNode(key,value); if(key<node->key) node->left=bst_insert(node->left,key,value); else if(key>node->key) node->right=bst_insert(node->right,key,value); else node->value=value; return node; }
void bst_inorder_collect(BSTNode* node, std::function<void(BSTNode*)> f){ if(!node) return; bst_inorder_collect(node->left,f); f(node); bst_inorder_collect(node->right,f); }
} // namespace dsa
