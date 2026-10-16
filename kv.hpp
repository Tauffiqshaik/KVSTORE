// Sharded key-value store with write-ahead log (WAL) crash recovery.
#pragma once
#include <string>
#include <unordered_map>
#include <map>
#include <vector>
#include <mutex>
#include <fstream>
#include <memory>
#include <optional>
#include <functional>
#include <cstdint>
#include <sstream>
#include <filesystem>

namespace kv {

// One shard: in-memory hash map + append-only WAL. Thread-safe.
class Shard {
public:
    explicit Shard(const std::string& wal_path) : path_(wal_path) {
        replay();
        wal_.open(path_, std::ios::app);
    }
    void put(const std::string& k, const std::string& v) {
        std::lock_guard<std::mutex> g(m_);
        append('P', k, v);
        data_[k] = v;
    }
    bool del(const std::string& k) {
        std::lock_guard<std::mutex> g(m_);
        if (!data_.count(k)) return false;
        append('D', k, "");
        data_.erase(k);
        return true;
    }
    std::optional<std::string> get(const std::string& k) {
        std::lock_guard<std::mutex> g(m_);
        auto it = data_.find(k);
        if (it == data_.end()) return std::nullopt;
        return it->second;
    }
    size_t size() { std::lock_guard<std::mutex> g(m_); return data_.size(); }

    // Rewrite WAL with only live entries (log compaction).
    void compact() {
        std::lock_guard<std::mutex> g(m_);
        wal_.close();
        std::string tmp = path_ + ".tmp";
        {
            std::ofstream o(tmp, std::ios::trunc);
            for (auto& [k, v] : data_) o << 'P' << ' ' << k.size() << ' ' << v.size() << '\n' << k << v << '\n';
            o.flush();
        }
        std::filesystem::rename(tmp, path_);  // atomic replace
        wal_.open(path_, std::ios::app);
    }
private:
    // Record: <op> <klen> <vlen>\n<key><value>\n  (length-prefixed, binary-safe)
    void append(char op, const std::string& k, const std::string& v) {
        wal_ << op << ' ' << k.size() << ' ' << v.size() << '\n' << k << v << '\n';
        wal_.flush();
    }
    void replay() {
        std::ifstream in(path_);
        if (!in) return;
        char op; size_t kl, vl;
        while (in >> op >> kl >> vl) {
            in.get();  // newline
            std::string k(kl, '\0'), v(vl, '\0');
            in.read(k.data(), kl); in.read(v.data(), vl); in.get();
            if (!in) break;  // torn/partial last record from a crash: ignore it
            if (op == 'P') data_[k] = v; else if (op == 'D') data_.erase(k);
        }
    }
    std::string path_;
    std::ofstream wal_;
    std::unordered_map<std::string, std::string> data_;
    std::mutex m_;
};

// Consistent-hash ring with virtual nodes -> minimal key movement on resharding.
class Store {
public:
    Store(const std::string& dir, int num_shards, int vnodes = 64) {
        std::filesystem::create_directories(dir);
        for (int i = 0; i < num_shards; ++i) {
            shards_.push_back(std::make_unique<Shard>(dir + "/shard" + std::to_string(i) + ".wal"));
            for (int v = 0; v < vnodes; ++v)
                ring_[hash("shard" + std::to_string(i) + "#" + std::to_string(v))] = i;
        }
    }
    void put(const std::string& k, const std::string& v) { shard_for(k).put(k, v); }
    bool del(const std::string& k) { return shard_for(k).del(k); }
    std::optional<std::string> get(const std::string& k) { return shard_for(k).get(k); }
    size_t size() { size_t n = 0; for (auto& s : shards_) n += s->size(); return n; }
    size_t shard_size(int i) { return shards_[i]->size(); }
    void compact() { for (auto& s : shards_) s->compact(); }
    int shard_index(const std::string& k) const {
        auto it = ring_.lower_bound(hash(k));
        if (it == ring_.end()) it = ring_.begin();
        return it->second;
    }
private:
    static uint64_t hash(const std::string& s) {  // FNV-1a 64-bit
        uint64_t h = 1469598103934665603ULL;
        for (unsigned char c : s) { h ^= c; h *= 1099511628211ULL; }
        return h;
    }
    Shard& shard_for(const std::string& k) { return *shards_[shard_index(k)]; }
    std::vector<std::unique_ptr<Shard>> shards_;
    std::map<uint64_t, int> ring_;
};

} // namespace kv
