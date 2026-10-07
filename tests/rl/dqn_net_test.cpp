// v0.9.32 G32: DqnNet 单元测试 — 结构校验 + 前向语义 + 坏权重拒绝
#include <gtest/gtest.h>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <vector>
#include "ai/rl/dqn_net.h"

using namespace rl;

namespace {

// 造一个最小合法权重: 2 层 2→2→1, 权重全 1 便于手算
void write_fixture(const char* path, bool corrupt_magic = false) {
    FILE* f = fopen(path, "wb");
    ASSERT_TRUE(f != nullptr);
    const char* magic = corrupt_magic ? "XXXX" : "RLDQ";
    fwrite(magic, 1, 4, f);
    uint32_t version = 1, n_layers = 2, pad = 0;
    fwrite(&version, 4, 1, f);
    fwrite(&n_layers, 4, 1, f);
    fwrite(&pad, 4, 1, f);
    // layer0: 2 -> 2, bias 0, weight 全 1
    uint32_t dims[2] = {2, 2};
    fwrite(dims, 4, 2, f);
    float zeros[2] = {0.0f, 0.0f};
    fwrite(zeros, 4, 2, f);
    float ones[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    fwrite(ones, 4, 4, f);
    // layer1: 2 -> 1, bias 0, weight 全 1
    uint32_t dims2[2] = {2, 1};
    fwrite(dims2, 4, 2, f);
    float z1[1] = {0.0f};
    fwrite(z1, 4, 1, f);
    float o2[2] = {1.0f, 1.0f};
    fwrite(o2, 4, 2, f);
    fclose(f);
}

} // namespace

TEST(DqnNet, RejectsMissingFile) {
    DqnNet net;
    EXPECT_FALSE(net.load("no_such_weights_file.bin"));
    EXPECT_FALSE(net.loaded());
}

TEST(DqnNet, RejectsBadMagic) {
    const char* path = "saves/_test_dqn_badmagic.bin";
    write_fixture(path, true);
    DqnNet net;
    EXPECT_FALSE(net.load(path));
    remove(path);
}

TEST(DqnNet, LoadsValidFixture) {
    const char* path = "saves/_test_dqn_ok.bin";
    write_fixture(path);
    DqnNet net;
    ASSERT_TRUE(net.load(path));
    EXPECT_TRUE(net.loaded());
    EXPECT_EQ(net.input_dim(), 2);
    EXPECT_EQ(net.output_dim(), 1);
    remove(path);
}

// 权重全 1、bias 0 的 2→2→1 网络:
//   第一层两个神经元输出均为 ReLU(x0+x1) → ReLU 后是 (s, s), s = ReLU(x0+x1)
//   第二层权重 [1,1] → 输出 ReLU(s) + ReLU(s) = 2s
// 即 output = 2 * ReLU(x0 + x1)
TEST(DqnNet, ForwardComputesExpected) {
    const char* path = "saves/_test_dqn_fwd.bin";
    write_fixture(path);
    DqnNet net;
    ASSERT_TRUE(net.load(path));

    std::vector<float> q;
    net.forward({1.0f, 2.0f}, q);
    ASSERT_EQ(q.size(), 1u);
    EXPECT_NEAR(q[0], 6.0f, 1e-5);          // 2 * ReLU(1+2)

    // ReLU: 输入全负 → 两层皆 0
    net.forward({-5.0f, -3.0f}, q);
    EXPECT_NEAR(q[0], 0.0f, 1e-5);

    // 混合: ReLU(-5+3) = 0 → output 0
    net.forward({-5.0f, 3.0f}, q);
    EXPECT_NEAR(q[0], 0.0f, 1e-5);

    net.forward({2.0f, 3.0f}, q);
    EXPECT_NEAR(q[0], 10.0f, 1e-5);         // 2 * ReLU(2+3)
    remove(path);
}

TEST(DqnNet, ForwardOnEmptyNetClearsOutput) {
    DqnNet net;
    std::vector<float> q{1.0f, 2.0f};
    net.forward({1.0f}, q);
    EXPECT_TRUE(q.empty());
}

TEST(DqnNet, RejectsDanglingLayer) {
    // layer0 输出 2 维, layer1 却要 3 维输入 → 首尾不相接必须拒绝
    const char* path = "saves/_test_dqn_dangling.bin";
    FILE* f = fopen(path, "wb");
    ASSERT_TRUE(f != nullptr);
    fwrite("RLDQ", 1, 4, f);
    uint32_t version = 1, n_layers = 2, pad = 0;
    fwrite(&version, 4, 1, f);
    fwrite(&n_layers, 4, 1, f);
    fwrite(&pad, 4, 1, f);
    uint32_t d0[2] = {2, 2};
    fwrite(d0, 4, 2, f);
    float z[2] = {0, 0}, w[4] = {1, 1, 1, 1};
    fwrite(z, 4, 2, f);
    fwrite(w, 4, 4, f);
    uint32_t d1[2] = {3, 1};               // 输入维3 ≠ 上一��输出维2
    fwrite(d1, 4, 2, f);
    float z1[1] = {0};
    fwrite(z1, 4, 1, f);
    float w1[3] = {1, 1, 1};
    fwrite(w1, 4, 3, f);
    fclose(f);

    DqnNet net;
    EXPECT_FALSE(net.load(path));
    remove(path);
}

// 真实产物: tools/rl/dqn_weights.bin (24144 字节, 3 层 18→64→64→10)
TEST(DqnNet, LoadsTrainedWeights) {
    DqnNet net;
    ASSERT_TRUE(net.load("tools/rl/dqn_weights.bin"))
        << "先跑 tools/rl/export_weights.py 生成权重";
    EXPECT_EQ(net.input_dim(), 18);
    EXPECT_EQ(net.output_dim(), 10);

    std::vector<float> q;
    net.forward(std::vector<float>(18, 0.0f), q);
    ASSERT_EQ(q.size(), 10u);
    for (float v : q) EXPECT_TRUE(std::isfinite(v));
}
