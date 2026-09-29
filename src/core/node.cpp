#include "node.h"

Node::~Node() {
    _propagate_exit_tree();
}

void Node::_set_tree(SceneTree* tree) {
    _tree = tree;
    for (auto& child : _children) {
        child->_set_tree(tree);
    }
}

void Node::_propagate_enter_tree() {
    if (_inside_tree) return;
    _inside_tree = true;
    _enter_tree();
    for (auto& child : _children) {
        child->_propagate_enter_tree();
    }
}

void Node::_propagate_ready() {
    _ready();
    for (auto& child : _children) {
        child->_propagate_ready();
    }
}

void Node::_propagate_exit_tree() {
    if (!_inside_tree) return;
    _exit_tree();
    _inside_tree = false;
    for (auto& child : _children) {
        child->_propagate_exit_tree();
    }
}

void Node::_propagate_process(double delta) {
    if (!_processing) return;
    _process(delta);
    for (auto& child : _children) {
        child->_propagate_process(delta);
    }
}
