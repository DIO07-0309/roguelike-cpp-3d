// B3-M: MirrorMemoryStore implementation
#include "ai/mirror/mirror_memory_store.h"

#include <nlohmann/json.hpp>

#include <cstdio>
#include <fstream>
#include <sstream>

namespace mirror {
namespace {
constexpr size_t kIntentCount = (size_t)PlayerIntention::COUNT;

using Counts = BehaviorCloneTable::Counts;

int total_of(const Counts& c) {
    int sum = 0;
    for (size_t i = 0; i < kIntentCount; i++) sum += c[i];
    return sum;
}

// 跨局遗忘曲线: 每次读入整体 ×0.99 (整数截断, 归零行由调用侧丢弃)
Counts decay_counts(const Counts& c) {
    Counts out{};
    for (size_t i = 0; i < kIntentCount; i++) {
        out[i] = (int)(c[i] * MirrorMemoryStore::kDecayPerLoad);
    }
    return out;
}

nlohmann::json counts_to_json(const Counts& c) {
    nlohmann::json arr = nlohmann::json::array();
    for (size_t i = 0; i < kIntentCount; i++) arr.push_back(c[i]);
    return arr;
}

bool json_to_counts(const nlohmann::json& arr, Counts& out) {
    if (!arr.is_array() || arr.size() != kIntentCount) return false;
    for (size_t i = 0; i < kIntentCount; i++) {
        if (!arr[i].is_number_integer()) return false;
        out[i] = arr[i].get<int>();
    }
    return true;
}

bool read_all(const std::string& file, std::string& out_text) {
    std::ifstream in(file, std::ios::binary);
    if (!in) return false;
    std::stringstream ss;
    ss << in.rdbuf();
    out_text = ss.str();
    return true;
}

// G22d: 快照表 → 落盘 json (全零行剔除, 桶组合截断到 kMaxEntries)
nlohmann::json build_snapshot(const BehaviorCloneTable& table) {
    nlohmann::json entries = nlohmann::json::object();
    size_t kept = 0;
    for (const auto& kv : table.table()) {
        if (kept >= MirrorMemoryStore::kMaxEntries) break;   // 熔丝: 桶组合最多 80
        if (total_of(kv.second) <= 0) continue;  // 零计数行不落盘
        entries[kv.first] = counts_to_json(kv.second);
        kept++;
    }
    nlohmann::json root;
    root["version"] = 1;
    root["entries"] = entries;
    return root;
}

// G22d: 先写 .tmp 再 rename 提升为正式档; 写失败或替换失败都清理 .tmp,
// 避免伪原子写留下永不读取的残留文件 (残留不会损坏正式档, 但会误导排查)
bool write_atomic(const std::string& text, const std::string& tmp,
                  const std::string& final) {
    std::ofstream out(tmp, std::ios::binary);
    if (!out) return false;
    out << text << "\n";
    out.close();
    if (!out) { std::remove(tmp.c_str()); return false; }   // 不得提升半成品
    if (std::rename(tmp.c_str(), final.c_str()) != 0) {
        std::remove(tmp.c_str());                            // 替换失败不留残留
        return false;
    }
    return true;
}
}  // namespace

std::string MirrorMemoryStore::_path = "saves/mirror_memory.json";

std::string MirrorMemoryStore::path() { return _path; }

void MirrorMemoryStore::set_path_for_test(const std::string& p) { _path = p; }

bool MirrorMemoryStore::load_into(BehaviorCloneTable& table) {
    std::string text;
    if (!read_all(_path, text)) return false;
    nlohmann::json root = nlohmann::json::parse(text, nullptr, false);
    if (root.is_discarded() || !root.is_object()) return false;
    if (!root.contains("entries") || !root["entries"].is_object()) return false;

    bool merged_any = false;
    for (auto it = root["entries"].begin(); it != root["entries"].end(); ++it) {
        Counts src{};
        if (!json_to_counts(it.value(), src)) continue;
        src = decay_counts(src);
        if (total_of(src) <= 0) continue;        // 衰减归零: 记忆自然过期
        table.merge_entry(it.key(), src);
        merged_any = true;
    }
    return merged_any;
}

bool MirrorMemoryStore::save_from(const BehaviorCloneTable& table) {
    nlohmann::json root = build_snapshot(table);
    return write_atomic(root.dump(), _path + ".tmp", _path);   // 伪原子替换
}

}  // namespace mirror
