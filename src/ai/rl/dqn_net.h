#pragma once
// v0.9.32G32: DqnNet — 手写 MLP 前向, 替代 ONNX Runtime。
//
// 为什么不用 ONNX Runtime: 本网络仅 3 层全连接 (18→64→64→10, 138 个参数),
// 手写前向约 80 行, 零第三方库 / 零 dll 依赖。引入 ONNX Runtime 需往 vendor/
// 加约 15MB 二进制, 违反项目约束 (vendor/ 仅允许 raylib + nlohmann/json)。
//
// 权重文件格式 (小端, 由 tools/rl/export_weights.py 生成):
//   "RLDQ" | version:u32 | n_layers:u32 | pad:u32
//   每层: in_dim:u32 | out_dim:u32 | bias[out_dim]:f32 | weight[out_dim*in_dim]:f32
//   权重索引 = o*in_dim + i (与 torch.nn.Linear 的 [out_dim, in_dim] 行优先一致)
//
// 推理语义: 网络只输出 10 维 Q 值, **不做动作选择**。argmax 与合法性 mask
// 由调用方负责 (见 DqnPolicy) —— 与 Python 侧 masked_argmax 保持同一套规则。
#include <cstdint>
#include <string>
#include <vector>

namespace rl {

class DqnNet {
public:
    DqnNet() = default;

    // 载入权重。失败返回 false 并保持空网络 (调用方须检查)
    bool load(const std::string& path);

    bool loaded() const { return !_layers.empty(); }
    int input_dim() const { return _layers.empty() ? 0 : _layers.front().in_dim; }
    int output_dim() const { return _layers.empty() ? 0 : _layers.back().out_dim; }

    // 前向: state 长度须等于 input_dim(), q_values 长度须等于 output_dim()
    // 逐层 Linear -> ReLU (最后一层不加ReLU)
    void forward(const std::vector<float>& state,
                 std::vector<float>& q_values) const;

private:
    struct Layer {
        int in_dim = 0;
        int out_dim = 0;
        std::vector<float> bias;
        std::vector<float> weight;   // out_dim * in_dim, 行优先
    };
    std::vector<Layer> _layers;
};

} // namespace rl
