// v0.9.32 G32: DqnNet 前向探针 — 给 tools/rl/verify_weights.py 做反向校验用
// 用法: dqn_probe.exe <weights.bin> <input.csv> <output.csv>
#include <cstdio>
#include <string>
#include <vector>
#include "ai/rl/dqn_net.h"

static bool read_input_csv(const char* path, int cols,
                           std::vector<std::vector<float>>& rows) {
    FILE* f = fopen(path, "r");
    if (!f) return false;
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        std::vector<float> row;
        const char* p = line;
        while (*p) {
            char* end = nullptr;
            float v = strtof(p, &end);
            if (end == p) break;
            row.push_back(v);
            p = end;
            if (*p == ',') p++;
        }
        if (!row.empty()) rows.push_back(row);
    }
    fclose(f);
    return !rows.empty() && cols > 0;
}

int main(int argc, char** argv) {
    if (argc < 4) {
        printf("usage: dqn_probe <weights.bin> <input.csv> <output.csv>\n");
        return 2;
    }
    rl::DqnNet net;
    if (!net.load(argv[1])) {
        printf("FAIL: cannot load weights %s\n", argv[1]);
        return 1;
    }

    std::vector<std::vector<float>> rows;
    if (!read_input_csv(argv[2], net.input_dim(), rows)) {
        printf("FAIL: cannot read input %s\n", argv[2]);
        return 1;
    }

    FILE* out = fopen(argv[3], "w");
    if (!out) { printf("FAIL: cannot write %s\n", argv[3]); return 1; }
    std::vector<float> q;
    for (auto& row : rows) {
        if ((int)row.size() != net.input_dim()) {
            fclose(out);
            printf("FAIL: input dim mismatch\n");
            return 1;
        }
        net.forward(row, q);
        for (size_t i = 0; i < q.size(); i++)
            fprintf(out, "%s%.7g", i ? "," : "", q[i]);
        fprintf(out, "\n");
    }
    fclose(out);
    printf("dqn_probe: %d rows forward OK (in=%d out=%d)\n",
           (int)rows.size(), net.input_dim(), net.output_dim());
    return 0;
}
