// v0.9.32 G32: DqnNet 实现 — 小端读取 + 逐层前向
#include "ai/rl/dqn_net.h"
#include <cstdio>
#include <cstring>

namespace rl {

namespace {
    constexpr char kMagic[4] = {'R', 'L', 'D', 'Q'};
    constexpr uint32_t kVersion = 1;

    bool read_exact(FILE* f, void* dst, size_t n) {
        return fread(dst, 1, n, f) == n;
    }

    bool read_u32(FILE* f, uint32_t& out) {
        return read_exact(f, &out, sizeof(uint32_t));
    }
} // namespace

// G53: 读取单层权重 (维度护栏 + bias/weight), false=读失败
bool DqnNet::_read_layer(FILE* f, Layer& L) {
    uint32_t in_dim = 0, out_dim = 0;
    if (!(read_u32(f, in_dim) && read_u32(f, out_dim))) return false;
    if (in_dim == 0 || out_dim == 0 || in_dim > 4096 || out_dim > 4096) return false;
    L.in_dim = (int)in_dim;
    L.out_dim = (int)out_dim;
    L.bias.resize(out_dim);
    L.weight.resize((size_t)out_dim * in_dim);
    return read_exact(f, L.bias.data(), out_dim * sizeof(float))
        && read_exact(f, L.weight.data(), L.weight.size() * sizeof(float));
}

// G53: 层间维度必须首尾相接, 否则前向会读到越界权重
bool DqnNet::_layers_are_chained(const std::vector<Layer>& layers) {
    for (size_t k = 1; k < layers.size(); k++) {
        if (layers[k].in_dim != layers[k - 1].out_dim) return false;
    }
    return true;
}

bool DqnNet::load(const std::string& path) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return false;

    char magic[4] = {};
    uint32_t version = 0, n_layers = 0, pad = 0;
    bool ok = read_exact(f, magic, 4) && read_u32(f, version)
              && read_u32(f, n_layers) && read_u32(f, pad);
    if (!ok || memcmp(magic, kMagic, 4) != 0 || version != kVersion) {
        fclose(f);
        return false;
    }

    std::vector<Layer> layers;
    layers.reserve(n_layers);
    for (uint32_t li = 0; li < n_layers && ok; li++) {
        Layer L;
        ok = _read_layer(f, L);
        if (ok) layers.push_back(std::move(L));
    }
    fclose(f);

    if (!ok || layers.empty()) return false;
    if (!_layers_are_chained(layers)) return false;
    _layers = std::move(layers);
    return true;
}

void DqnNet::forward(const std::vector<float>& state,
                     std::vector<float>& q_values) const {
    if (_layers.empty()) { q_values.clear(); return; }

    std::vector<float> cur = state;
    std::vector<float> next;
    for (size_t li = 0; li < _layers.size(); li++) {
        const Layer& L = _layers[li];
        next.assign(L.out_dim, 0.0f);
        for (int o = 0; o < L.out_dim; o++) {
            float sum = L.bias[o];
            const float* w = L.weight.data() + (size_t)o * L.in_dim;
            for (int i = 0; i < L.in_dim; i++)
                sum += w[i] * cur[i];
            // 最后一层不加 ReLU
            next[o] = (li + 1 < _layers.size() && sum < 0.0f) ? 0.0f : sum;
        }
        cur.swap(next);
    }
    q_values = std::move(cur);
}

} // namespace rl
