// v0.9.32 G32: DqnPolicy 探针 — 给 tools/rl/verify_policy.py 做一致性校验
// 用法: dqn_policy_probe.exe <weights.bin> <input.txt> <output.txt>
//   input 每行:  obs8(逗号分隔) "|" mask10(逗号分隔 0/1)
//   output 首行: build_input 得到的 18 维 + ";" + 选中的动作 id
#include <cstdio>
#include <sstream>
#include <string>
#include <vector>
#include "ai/rl/dqn_policy.h"

static bool parse_line(const std::string& line,
                       std::vector<float>& obs,
                       std::vector<float>& mask) {
    size_t bar = line.find('|');
    if (bar == std::string::npos) return false;
    auto split = [](const std::string& s, std::vector<float>& out) {
        std::stringstream ss(s);
        std::string tok;
        out.clear();
        while (std::getline(ss, tok, ','))
            if (!tok.empty()) out.push_back(strtof(tok.c_str(), nullptr));
    };
    split(line.substr(0, bar), obs);
    split(line.substr(bar + 1), mask);
    return obs.size() == 8 && mask.size() == 10;
}

int main(int argc, char** argv) {
    if (argc < 4) {
        printf("usage: dqn_policy_probe <weights.bin> <input.txt> <output.txt>\n");
        return 2;
    }
    rl::DqnPolicy policy;
    if (!policy.load(argv[1])) {
        printf("FAIL: cannot load weights %s\n", argv[1]);
        return 1;
    }

    FILE* in = fopen(argv[2], "r");
    if (!in) { printf("FAIL: cannot read %s\n", argv[2]); return 1; }
    FILE* out = fopen(argv[3], "w");
    if (!out) { fclose(in); printf("FAIL: cannot write %s\n", argv[3]); return 1; }

    fprintf(out, "input18;action\n");   // 表头, Python 侧 skiprows=1
    char line[512];
    int count = 0;
    while (fgets(line, sizeof(line), in)) {
        std::string s(line);
        while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
        if (s.empty()) continue;
        std::vector<float> obs, mask;
        if (!parse_line(s, obs, mask)) continue;
        auto x = rl::DqnPolicy::build_input(obs, mask);
        int act = policy.select_action(obs, mask);
        for (size_t i = 0; i < x.size(); i++)
            fprintf(out, "%s%.7g", i ? "," : "", x[i]);
        fprintf(out, ";%d\n", act);
        count++;
    }
    fclose(in);
    fclose(out);
    printf("dqn_policy_probe: %d rows OK (in=%d)\n", count, policy.input_dim());
    return 0;
}
