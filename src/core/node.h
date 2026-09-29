#pragma once
#include <vector>
#include <memory>
#include <string>
#include <algorithm>
#include "object.h"

// ============================================================
// Node — 场景树节点 (Godot 风格)
//
// 生命周期:
//   _enter_tree() → _ready() → _process(dt) → ... → _exit_tree()
// ============================================================
class SceneTree;

class Node : public Object {
public:
    std::string name;

    Node() = default;
    virtual ~Node();

    // ---- 树管理 ----
    Node* get_parent() const { return _parent; }
    std::vector<std::shared_ptr<Node>>& get_children() { return _children; }

    // 路径查找: get_node("HUD/SkillBar")
    template<typename T = Node>
    T* get_node(const std::string& path) const;

    // ---- 生命周期 (SceneTree 调用) ----
    virtual void _enter_tree() {}
    virtual void _ready() {}
    virtual void _process(double delta) {}
    virtual void _render() {}
    virtual void _input(const class InputMap& input) {}
    virtual void _exit_tree() {}

    // ---- 场景树访问 ----
    SceneTree* get_tree() const { return _tree; }
    bool is_inside_tree() const { return _inside_tree; }

    // ---- 暂停 ----
    void set_process(bool enabled) { _processing = enabled; }
    bool is_processing() const { return _processing; }

    // ---- BGM 映射 (各场景重写返回自己的 BGM 名) ----
    virtual const char* get_bgm_name() const { return nullptr; }

protected:
    Node* _parent = nullptr;
    std::vector<std::shared_ptr<Node>> _children;
    SceneTree* _tree = nullptr;
    bool _processing = true;
    bool _inside_tree = false;
    bool _queued_free = false;

    friend class SceneTree;
    void _set_tree(SceneTree* tree);
    void _propagate_enter_tree();
    void _propagate_ready();
    void _propagate_exit_tree();
    void _propagate_process(double delta);
};

// 路径查找实现
template<typename T>
T* Node::get_node(const std::string& path) const {
    if (path.empty()) return nullptr;

    // 支持 ".." 和 "/" 路径
    const Node* current = this;
    std::string remaining = path;

    // ".." 回父节点
    while (remaining.size() >= 2 && remaining[0] == '.' && remaining[1] == '.') {
        remaining = remaining.substr(2);
        if (!remaining.empty() && remaining[0] == '/') remaining = remaining.substr(1);
        if (current->_parent) current = current->_parent;
        else return nullptr;
    }
    if (remaining.empty())
        return dynamic_cast<T*>(const_cast<Node*>(current));

    // 在子节点中按名查找
    for (auto& child : current->_children) {
        if (child->name == remaining) {
            return dynamic_cast<T*>(child.get());
        }
    }

    // 支持 "A/B/C" 路径
    size_t slash = remaining.find('/');
    if (slash != std::string::npos) {
        std::string first = remaining.substr(0, slash);
        for (auto& child : current->_children) {
            if (child->name == first) {
                return child->template get_node<T>(remaining.substr(slash + 1));
            }
        }
    }

    return nullptr;
}
