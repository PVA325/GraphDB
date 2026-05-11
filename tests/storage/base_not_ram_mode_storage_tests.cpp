#include <gtest/gtest.h>
#include <filesystem>
#include <string>

#include "graph_db.hpp"

using namespace storage;

class DiskModeTest : public ::testing::Test {
protected:
  std::string dir_;

  void SetUp() override {
    std::string tmpl = (std::filesystem::temp_directory_path() / "graphdb_test_XXXXXX").string();
    char* result = mkdtemp(tmpl.data());
    ASSERT_NE(result, nullptr) << "mkdtemp failed";
    dir_ = result;
  }

  void TearDown() override {
    std::filesystem::remove_all(dir_);
  }

  GraphDB open() { return GraphDB(dir_); }

  static Properties props(std::initializer_list<std::pair<std::string, Value>> list) {
    Properties p;
    for (auto& [k, v] : list) p[k] = v;
    return p;
  }
};

TEST_F(DiskModeTest, CreateAndGetNode) {
  GraphDB db(dir_);
  NodeId id = db.create_node({"Person"}, props({{"age", Value{int64_t(30)}}}));
  Node* node = db.get_node(id);
  ASSERT_NE(node, nullptr);
  EXPECT_TRUE(node->alive);
  EXPECT_EQ(node->labels, std::vector<std::string>{"Person"});
  EXPECT_EQ(std::get<int64_t>(node->properties.at("age")), 30);
}

TEST_F(DiskModeTest, DeleteNode) {
  GraphDB db(dir_);
  NodeId id = db.create_node({"X"}, {});
  db.delete_node(id);
  EXPECT_EQ(db.get_node(id), nullptr);
}

TEST_F(DiskModeTest, NodeIdReuse) {
  GraphDB db(dir_);
  NodeId a = db.create_node({}, {});
  NodeId b = db.create_node({}, {});
  db.delete_node(a);
  NodeId c = db.create_node({}, {});
  EXPECT_EQ(c, a);
  (void)b;
}

TEST_F(DiskModeTest, SetNodeProperty) {
  GraphDB db(dir_);
  NodeId id = db.create_node({}, props({{"k", Value{int64_t(1)}}}));
  db.set_node_property(id, "k", Value{int64_t(2)});
  Node* node = db.get_node(id);
  ASSERT_NE(node, nullptr);
  EXPECT_EQ(std::get<int64_t>(node->properties.at("k")), 2);
}

TEST_F(DiskModeTest, SetNodeLabel) {
  GraphDB db(dir_);
  NodeId id = db.create_node({}, {});
  db.set_node_label(id, "Animal");
  Node* node = db.get_node(id);
  ASSERT_NE(node, nullptr);
  ASSERT_EQ(node->labels.size(), 1u);
  EXPECT_EQ(node->labels[0], "Animal");
}

TEST_F(DiskModeTest, DeleteNodeLabel) {
  GraphDB db(dir_);
  NodeId id = db.create_node({"A", "B"}, {});
  db.delete_node_label(id, "A");
  Node* node = db.get_node(id);
  ASSERT_NE(node, nullptr);
  EXPECT_EQ(node->labels.size(), 1u);
  EXPECT_EQ(node->labels[0], "B");
}

TEST_F(DiskModeTest, CreateAndGetEdge) {
  GraphDB db(dir_);
  NodeId a = db.create_node({}, {});
  NodeId b = db.create_node({}, {});
  EdgeId eid = db.create_edge(a, b, "KNOWS", props({{"since", Value{int64_t(2020)}}}));
  Edge* edge = db.get_edge(eid);
  ASSERT_NE(edge, nullptr);
  EXPECT_TRUE(edge->alive);
  EXPECT_EQ(edge->src, a);
  EXPECT_EQ(edge->dst, b);
  EXPECT_EQ(edge->type, "KNOWS");
  EXPECT_EQ(std::get<int64_t>(edge->properties.at("since")), 2020);
}

TEST_F(DiskModeTest, DeleteEdge) {
  GraphDB db(dir_);
  NodeId a = db.create_node({}, {});
  NodeId b = db.create_node({}, {});
  EdgeId eid = db.create_edge(a, b, "T", {});
  db.delete_edge(eid);
  EXPECT_EQ(db.get_edge(eid), nullptr);
}

TEST_F(DiskModeTest, SetEdgeType) {
  GraphDB db(dir_);
  NodeId a = db.create_node({}, {});
  NodeId b = db.create_node({}, {});
  EdgeId eid = db.create_edge(a, b, "OLD", {});
  db.set_edge_type(eid, "NEW");
  EXPECT_EQ(db.get_edge(eid)->type, "NEW");
}

TEST_F(DiskModeTest, CascadeDeleteEdgesOnNodeDelete) {
  GraphDB db(dir_);
  NodeId a = db.create_node({}, {});
  NodeId b = db.create_node({}, {});
  EdgeId eid = db.create_edge(a, b, "T", {});
  db.delete_node(a);
  EXPECT_EQ(db.get_edge(eid), nullptr);
}

TEST_F(DiskModeTest, NodesByLabel) {
  GraphDB db(dir_);
  NodeId a = db.create_node({"Person"}, {});
  NodeId b = db.create_node({"Person"}, {});
  NodeId c = db.create_node({"Animal"}, {});

  auto cursor = db.nodes_with_label("Person");
  std::vector<NodeId> found;
  Node* n;
  while (cursor->next(n)) found.push_back(n->id);

  EXPECT_EQ(found.size(), 2u);
  EXPECT_NE(std::find(found.begin(), found.end(), a), found.end());
  EXPECT_NE(std::find(found.begin(), found.end(), b), found.end());
  (void)c;
}

TEST_F(DiskModeTest, NodesByProperty) {
  GraphDB db(dir_);
  NodeId a = db.create_node({}, props({{"age", Value{int64_t(25)}}}));
  NodeId b = db.create_node({}, props({{"age", Value{int64_t(30)}}}));

  auto cursor = db.nodes_with_property("age", Value{int64_t(25)});
  Node* n;
  ASSERT_TRUE(cursor->next(n));
  EXPECT_EQ(n->id, a);
  EXPECT_FALSE(cursor->next(n));
  (void)b;
}

TEST_F(DiskModeTest, PropertyIndexUpdatedOnSetProperty) {
  GraphDB db(dir_);
  NodeId id = db.create_node({}, props({{"k", Value{int64_t(1)}}}));

  db.set_node_property(id, "k", Value{int64_t(2)});

  auto c1 = db.nodes_with_property("k", Value{int64_t(1)});
  Node* n;
  EXPECT_FALSE(c1->next(n));

  auto c2 = db.nodes_with_property("k", Value{int64_t(2)});
  ASSERT_TRUE(c2->next(n));
  EXPECT_EQ(n->id, id);
}

TEST_F(DiskModeTest, OutgoingEdges) {
  GraphDB db(dir_);
  NodeId a = db.create_node({}, {});
  NodeId b = db.create_node({}, {});
  NodeId c = db.create_node({}, {});
  EdgeId e1 = db.create_edge(a, b, "T", {});
  EdgeId e2 = db.create_edge(a, c, "T", {});

  auto cursor = db.outgoing_edges(a);
  std::vector<EdgeId> found;
  Edge* e;
  while (cursor->next(e)) found.push_back(e->id);

  EXPECT_EQ(found.size(), 2u);
  EXPECT_NE(std::find(found.begin(), found.end(), e1), found.end());
  EXPECT_NE(std::find(found.begin(), found.end(), e2), found.end());
}

TEST_F(DiskModeTest, IncomingEdges) {
  GraphDB db(dir_);
  NodeId a = db.create_node({}, {});
  NodeId b = db.create_node({}, {});
  EdgeId eid = db.create_edge(a, b, "T", {});

  auto cursor = db.incoming_edges(b);
  Edge* e;
  ASSERT_TRUE(cursor->next(e));
  EXPECT_EQ(e->id, eid);
}

TEST_F(DiskModeTest, EdgesByType) {
  GraphDB db(dir_);
  NodeId a = db.create_node({}, {});
  NodeId b = db.create_node({}, {});
  db.create_edge(a, b, "KNOWS", {});
  db.create_edge(a, b, "LIKES", {});

  EXPECT_EQ(db.edge_count_with_type("KNOWS"), 1u);
  EXPECT_EQ(db.edge_count_with_type("LIKES"), 1u);
  EXPECT_EQ(db.edge_count_with_type("OTHER"), 0u);
}

TEST_F(DiskModeTest, NodeCount) {
  GraphDB db(dir_);
  EXPECT_EQ(db.node_count(), 0u);
  NodeId a = db.create_node({}, {});
  NodeId b = db.create_node({}, {});
  EXPECT_EQ(db.node_count(), 2u);
  db.delete_node(a);
  EXPECT_EQ(db.node_count(), 1u);
  (void)b;
}

TEST_F(DiskModeTest, NodeCountWithLabel) {
  GraphDB db(dir_);
  db.create_node({"Person"}, {});
  db.create_node({"Person"}, {});
  db.create_node({"Animal"}, {});
  EXPECT_EQ(db.node_count_with_label("Person"), 2u);
  EXPECT_EQ(db.node_count_with_label("Animal"), 1u);
}

TEST_F(DiskModeTest, AvgOutDegree) {
  GraphDB db(dir_);
  NodeId a = db.create_node({"Person"}, {});
  NodeId b = db.create_node({"Person"}, {});
  NodeId c = db.create_node({}, {});
  db.create_edge(a, c, "T", {});
  db.create_edge(a, c, "T", {});
  db.create_edge(b, c, "T", {});
  EXPECT_DOUBLE_EQ(db.avg_out_degree("Person"), 1.5);
}

TEST_F(DiskModeTest, PropertyCountWithLabel) {
  GraphDB db(dir_);
  db.create_node({"Person"}, props({{"age", Value{int64_t(25)}}}));
  db.create_node({"Person"}, props({{"age", Value{int64_t(25)}}}));
  db.create_node({"Person"}, props({{"age", Value{int64_t(30)}}}));

  EXPECT_EQ(db.property_count("age", Value{int64_t(25)}, "Person"), 2u);
  EXPECT_EQ(db.property_count("age", Value{int64_t(30)}, "Person"), 1u);
}

TEST_F(DiskModeTest, PropertyCountNoLabel) {
  GraphDB db(dir_);
  db.create_node({}, props({{"k", Value{int64_t(1)}}}));
  db.create_node({}, props({{"k", Value{int64_t(1)}}}));
  EXPECT_EQ(db.property_count("k", Value{int64_t(1)}, ""), 2u);
}

TEST_F(DiskModeTest, PropertyCountUpdatedOnSetProperty) {
  GraphDB db(dir_);
  NodeId id = db.create_node({}, props({{"k", Value{int64_t(1)}}}));
  EXPECT_EQ(db.property_count("k", Value{int64_t(1)}, ""), 1u);
  db.set_node_property(id, "k", Value{int64_t(2)});
  EXPECT_EQ(db.property_count("k", Value{int64_t(1)}, ""), 0u);
  EXPECT_EQ(db.property_count("k", Value{int64_t(2)}, ""), 1u);
}

TEST_F(DiskModeTest, AllNodesCursor) {
  GraphDB db(dir_);
  NodeId a = db.create_node({}, {});
  NodeId b = db.create_node({}, {});
  NodeId c = db.create_node({}, {});
  db.delete_node(b);

  auto cursor = db.all_nodes();
  std::vector<NodeId> found;
  Node* n;
  while (cursor->next(n)) found.push_back(n->id);

  EXPECT_EQ(found.size(), 2u);
  EXPECT_NE(std::find(found.begin(), found.end(), a), found.end());
  EXPECT_NE(std::find(found.begin(), found.end(), c), found.end());
  EXPECT_EQ(std::find(found.begin(), found.end(), b), found.end());
}

TEST_F(DiskModeTest, AllNodesCursorWithPredicate) {
  GraphDB db(dir_);
  db.create_node({}, props({{"age", Value{int64_t(20)}}}));
  db.create_node({}, props({{"age", Value{int64_t(30)}}}));
  db.create_node({}, props({{"age", Value{int64_t(40)}}}));

  auto cursor = db.all_nodes([](Node* n) {
    auto it = n->properties.find("age");
    return it != n->properties.end() && std::get<int64_t>(it->second) > 25;
  });

  size_t count = 0;
  Node* n;
  while (cursor->next(n)) ++count;
  EXPECT_EQ(count, 2u);
}

TEST_F(DiskModeTest, AllNodesCursorWithLimit) {
  GraphDB db(dir_);
  for (int i = 0; i < 10; ++i) db.create_node({}, {});

  auto cursor = db.all_nodes(nullptr, 3);
  size_t count = 0;
  Node* n;
  while (cursor->next(n)) ++count;
  EXPECT_EQ(count, 3u);
}

TEST_F(DiskModeTest, AllValueTypes) {
  GraphDB db(dir_);
  NodeId id = db.create_node({}, props({
                                         {"i", Value{int64_t(42)}},
                                         {"d", Value{3.14}},
                                         {"s", Value{std::string("hello")}},
                                         {"b", Value{true}}
                                       }));

  Node* n = db.get_node(id);
  ASSERT_NE(n, nullptr);
  EXPECT_EQ(std::get<int64_t>    (n->properties.at("i")), 42);
  EXPECT_DOUBLE_EQ(std::get<double>(n->properties.at("d")), 3.14);
  EXPECT_EQ(std::get<std::string>(n->properties.at("s")), "hello");
  EXPECT_EQ(std::get<bool>       (n->properties.at("b")), true);
}