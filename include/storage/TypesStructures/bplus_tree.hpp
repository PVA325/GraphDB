#pragma once

#include <algorithm>
#include <vector>
#include <memory>
#include <optional>
#include <iostream>

namespace storage {

  template <typename Key, typename Value, size_t Order = 16>
  class BPlusTree {
  private:
    struct Node {
      bool is_leaf;
      Node(bool leaf) : is_leaf(leaf) {}
      virtual ~Node() = default;
    };

    struct InternalNode : public Node {
      std::vector<Key> keys;
      std::vector<std::unique_ptr<Node>> children;
      InternalNode() : Node(false) {}
    };

    struct LeafNode : public Node {
      std::vector<Key> keys;
      std::vector<Value> values;
      LeafNode* next = nullptr;
      LeafNode* prev = nullptr;
      LeafNode() : Node(true) {}
    };

    std::unique_ptr<Node> root_;
    LeafNode* head_leaf_ = nullptr;

  public:
    BPlusTree() {
      auto root = std::make_unique<LeafNode>();
      head_leaf_ = root.get();
      root_ = std::move(root);
    }

    Value* find(const Key& key) {
      LeafNode* leaf = find_leaf(root_.get(), key);
      auto it = std::lower_bound(leaf->keys.begin(), leaf->keys.end(), key);
      if (it != leaf->keys.end() && *it == key) {
        return &leaf->values[std::distance(leaf->keys.begin(), it)];
      }
      return nullptr;
    }

    // Возвращает итератор или указатель на первый лист для диапазонных запросов
    void get_range(const Key& start, const Key& end, std::vector<Value>& result) {
      LeafNode* leaf = find_leaf(root_.get(), start);
      bool keep_scanning = true;
      while (leaf && keep_scanning) {
        for (size_t i = 0; i < leaf->keys.size(); ++i) {
          if (leaf->keys[i] >= start && leaf->keys[i] <= end) {
            result.push_back(leaf->values[i]);
          }
          if (leaf->keys[i] > end) {
            keep_scanning = false;
            break;
          }
        }
        leaf = leaf->next;
      }
    }

    void insert(const Key& key, const Value& value) {
      Node* root = root_.get();
      LeafNode* leaf = find_leaf(root, key);

      auto it = std::lower_bound(leaf->keys.begin(), leaf->keys.end(), key);
      if (it != leaf->keys.end() && *it == key) {
        // Ключ уже существует, обновляем/дополняем значение (например, push_back в vector)
        leaf->values[std::distance(leaf->keys.begin(), it)] = value;
        return;
      }

      size_t idx = std::distance(leaf->keys.begin(), it);
      leaf->keys.insert(it, key);
      leaf->values.insert(leaf->values.begin() + idx, value);

      if (leaf->keys.size() >= Order) {
        split_leaf(leaf);
      }
    }

    void clear() {
      root_.reset();
      auto root = std::make_unique<LeafNode>();
      head_leaf_ = root.get();
      root_ = std::move(root);
    }

  private:
    LeafNode* find_leaf(Node* node, const Key& key) {
      while (!node->is_leaf) {
        auto* internal = static_cast<InternalNode*>(node);
        auto it = std::upper_bound(internal->keys.begin(), internal->keys.end(), key);
        size_t idx = std::distance(internal->keys.begin(), it);
        node = internal->children[idx].get();
      }
      return static_cast<LeafNode*>(node);
    }

    void split_leaf(LeafNode* leaf) {
      auto new_leaf = std::make_unique<LeafNode>();
      size_t split_idx = leaf->keys.size() / 2;

      new_leaf->keys.assign(std::make_move_iterator(leaf->keys.begin() + split_idx),
                            std::make_move_iterator(leaf->keys.end()));
      new_leaf->values.assign(std::make_move_iterator(leaf->values.begin() + split_idx),
                              std::make_move_iterator(leaf->values.end()));

      leaf->keys.erase(leaf->keys.begin() + split_idx, leaf->keys.end());
      leaf->values.erase(leaf->values.begin() + split_idx, leaf->values.end());

      new_leaf->next = leaf->next;
      new_leaf->prev = leaf;
      if (leaf->next) leaf->next->prev = new_leaf.get();
      leaf->next = new_leaf.get();

      Key split_key = new_leaf->keys.front();
      insert_into_parent(leaf, split_key, std::move(new_leaf));
    }

    void insert_into_parent(Node* old_node, const Key& key, std::unique_ptr<Node> new_node) {
      if (old_node == root_.get()) {
        auto new_root = std::make_unique<InternalNode>();
        new_root->keys.push_back(key);
        new_root->children.push_back(std::move(root_));
        new_root->children.push_back(std::move(new_node));
        root_ = std::move(new_root);
        return;
      }

      InternalNode* parent = find_parent(root_.get(), old_node);
      auto it = std::lower_bound(parent->keys.begin(), parent->keys.end(), key);
      size_t idx = std::distance(parent->keys.begin(), it);

      parent->keys.insert(it, key);
      parent->children.insert(parent->children.begin() + idx + 1, std::move(new_node));

      if (parent->keys.size() >= Order) {
        split_internal(parent);
      }
    }

    InternalNode* find_parent(Node* current, Node* target) {
      if (current->is_leaf) return nullptr;
      auto* internal = static_cast<InternalNode*>(current);
      for (const auto& child : internal->children) {
        if (child.get() == target) return internal;
        if (!child->is_leaf) {
          auto* res = find_parent(child.get(), target);
          if (res) return res;
        }
      }
      return nullptr;
    }

    void split_internal(InternalNode* internal) {
      auto new_internal = std::make_unique<InternalNode>();
      size_t split_idx = internal->keys.size() / 2;

      Key push_up_key = internal->keys[split_idx];

      new_internal->keys.assign(std::make_move_iterator(internal->keys.begin() + split_idx + 1),
                                std::make_move_iterator(internal->keys.end()));
      new_internal->children.assign(std::make_move_iterator(internal->children.begin() + split_idx + 1),
                                    std::make_move_iterator(internal->children.end()));

      internal->keys.erase(internal->keys.begin() + split_idx, internal->keys.end());
      internal->children.erase(internal->children.begin() + split_idx + 1, internal->children.end());

      insert_into_parent(internal, push_up_key, std::move(new_internal));
    }
  };

} // namespace storage