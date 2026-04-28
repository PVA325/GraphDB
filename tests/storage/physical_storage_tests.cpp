#include <gtest/gtest.h>
#include "storage.hpp"

using namespace storage;
static Properties make_props(std::initializer_list<std::pair<std::string, Value>> pairs) {
  Properties p;
  for (auto& [k, v] : pairs) p[k] = v;
  return p;
}

TEST(NodeCrud, CreateBasic) {
  GraphDB db;
  NodeId id = db.create_node({"Person"}, make_props({{"name", Value{"Alice"}}, {"age", Value{int64_t(30)}}}));
  Node* n = db.get_node(id);
  ASSERT_NE(n, nullptr);
  EXPECT_TRUE(n->alive);
  EXPECT_EQ(n->id, id);
  EXPECT_EQ(n->labels.size(), 1u);
  EXPECT_EQ(n->labels[0], "Person");
  EXPECT_EQ(std::get<std::string>(n->properties.at("name")), "Alice");
  EXPECT_EQ(std::get<int64_t>(n->properties.at("age")), 30);
}

TEST(NodeCrud, CreateNoLabelsNoProps) {
  GraphDB db;
  NodeId id = db.create_node({}, {});
  Node* n = db.get_node(id);
  ASSERT_NE(n, nullptr);
  EXPECT_TRUE(n->labels.empty());
  EXPECT_TRUE(n->properties.empty());
}

TEST(NodeCrud, CreateMultipleLabels) {
  GraphDB db;
  NodeId id = db.create_node({"A", "B", "C"}, {});
  Node* n = db.get_node(id);
  ASSERT_NE(n, nullptr);
  EXPECT_EQ(n->labels.size(), 3u);
}

TEST(NodeCrud, IdsAreSequential) {
  GraphDB db;
  NodeId a = db.create_node({}, {});
  NodeId b = db.create_node({}, {});
  NodeId c = db.create_node({}, {});
  EXPECT_EQ(a, 0u);
  EXPECT_EQ(b, 1u);
  EXPECT_EQ(c, 2u);
}

TEST(NodeCrud, GetNonExistent) {
  GraphDB db;
  EXPECT_EQ(db.get_node(999), nullptr);
}

TEST(NodeCrud, DeleteNode) {
  GraphDB db;
  NodeId id = db.create_node({"X"}, {});
  db.delete_node(id);
  EXPECT_EQ(db.get_node(id), nullptr);
}

TEST(NodeCrud, DeleteNodeReusesId) {
  GraphDB db;
  NodeId a = db.create_node({}, {});
  db.delete_node(a);
  NodeId b = db.create_node({}, {});

  EXPECT_EQ(a, b);
}

TEST(NodeCrud, NodeCountAfterCreateDelete) {
  GraphDB db;
  EXPECT_EQ(db.node_count(), 0u);
  NodeId a = db.create_node({}, {});
  NodeId b = db.create_node({}, {});
  EXPECT_EQ(db.node_count(), 2u);
  db.delete_node(a);
  EXPECT_EQ(db.node_count(), 1u);
  db.delete_node(b);
  EXPECT_EQ(db.node_count(), 0u);
}

TEST(NodeCrud, SetNodeProperty) {
  GraphDB db;
  NodeId id = db.create_node({}, make_props({{"x", Value{int64_t(1)}}}));
  db.set_node_property(id, "x", Value{int64_t(42)});
  Node* n = db.get_node(id);
  EXPECT_EQ(std::get<int64_t>(n->properties.at("x")), 42);
}

TEST(NodeCrud, SetNodePropertyNew) {
  GraphDB db;
  NodeId id = db.create_node({}, {});
  db.set_node_property(id, "new_key", Value{true});
  Node* n = db.get_node(id);
  EXPECT_EQ(std::get<bool>(n->properties.at("new_key")), true);
}

TEST(NodeCrud, SetNodeLabel) {
  GraphDB db;
  NodeId id = db.create_node({}, {});
  db.set_node_label(id, "Tag");
  Node* n = db.get_node(id);
  ASSERT_EQ(n->labels.size(), 1u);
  EXPECT_EQ(n->labels[0], "Tag");
}

TEST(NodeCrud, SetNodeLabelIdempotent) {
  GraphDB db;
  NodeId id = db.create_node({"Tag"}, {});
  db.set_node_label(id, "Tag");
  Node* n = db.get_node(id);
  EXPECT_EQ(n->labels.size(), 1u);
}

TEST(NodeCrud, DeleteNodeLabel) {
  GraphDB db;
  NodeId id = db.create_node({"A", "B"}, {});
  db.delete_node_label(id, "A");
  Node* n = db.get_node(id);
  ASSERT_EQ(n->labels.size(), 1u);
  EXPECT_EQ(n->labels[0], "B");
}

TEST(EdgeCrud, CreateBasic) {
  GraphDB db;
  NodeId a = db.create_node({}, {});
  NodeId b = db.create_node({}, {});
  EdgeId eid = db.create_edge(a, b, "KNOWS", {});
  Edge* e = db.get_edge(eid);
  ASSERT_NE(e, nullptr);
  EXPECT_TRUE(e->alive);
  EXPECT_EQ(e->src, a);
  EXPECT_EQ(e->dst, b);
  EXPECT_EQ(e->type, "KNOWS");
}

TEST(EdgeCrud, GetNonExistent) {
  GraphDB db;
  EXPECT_EQ(db.get_edge(999), nullptr);
}

TEST(EdgeCrud, DeleteEdge) {
  GraphDB db;
  NodeId a = db.create_node({}, {});
  NodeId b = db.create_node({}, {});
  EdgeId eid = db.create_edge(a, b, "T", {});
  db.delete_edge(eid);
  EXPECT_EQ(db.get_edge(eid), nullptr);
}

TEST(EdgeCrud, DeleteEdgeReusesId) {
  GraphDB db;
  NodeId a = db.create_node({}, {});
  NodeId b = db.create_node({}, {});
  EdgeId e1 = db.create_edge(a, b, "T", {});
  db.delete_edge(e1);
  EdgeId e2 = db.create_edge(a, b, "T", {});
  EXPECT_EQ(e1, e2);
}

TEST(EdgeCrud, SetEdgeProperty) {
  GraphDB db;
  NodeId a = db.create_node({}, {});
  NodeId b = db.create_node({}, {});
  EdgeId eid = db.create_edge(a, b, "T", {});
  db.set_edge_property(eid, "weight", Value{3.14});
  Edge* e = db.get_edge(eid);
  EXPECT_DOUBLE_EQ(std::get<double>(e->properties.at("weight")), 3.14);
}

TEST(EdgeCrud, SetEdgeType) {
  GraphDB db;
  NodeId a = db.create_node({}, {});
  NodeId b = db.create_node({}, {});
  EdgeId eid = db.create_edge(a, b, "OLD", {});
  db.set_edge_type(eid, "NEW");
  Edge* e = db.get_edge(eid);
  EXPECT_EQ(e->type, "NEW");
  EXPECT_EQ(db.edge_count_with_type("OLD"), 0u);
  EXPECT_EQ(db.edge_count_with_type("NEW"), 1u);
}

TEST(EdgeCrud, DeleteNodeCascadesEdges) {
  GraphDB db;
  NodeId a = db.create_node({}, {});
  NodeId b = db.create_node({}, {});
  EdgeId eid = db.create_edge(a, b, "T", {});
  db.delete_node(a);
  EXPECT_EQ(db.get_edge(eid), nullptr);
}

TEST(EdgeCrud, SelfLoop) {
  GraphDB db;
  NodeId a = db.create_node({}, {});
  EdgeId eid = db.create_edge(a, a, "SELF", {});
  Edge* e = db.get_edge(eid);
  ASSERT_NE(e, nullptr);
  EXPECT_EQ(e->src, a);
  EXPECT_EQ(e->dst, a);
}

TEST(Index, NodeCountWithLabel) {
  GraphDB db;
  db.create_node({"Person"}, {});
  db.create_node({"Person"}, {});
  db.create_node({"Robot"}, {});
  EXPECT_EQ(db.node_count_with_label("Person"), 2u);
  EXPECT_EQ(db.node_count_with_label("Robot"), 1u);
  EXPECT_EQ(db.node_count_with_label("Ghost"), 0u);
}

TEST(Index, NodeCountWithLabelAfterDelete) {
  GraphDB db;
  NodeId a = db.create_node({"Person"}, {});
  db.create_node({"Person"}, {});
  db.delete_node(a);
  EXPECT_EQ(db.node_count_with_label("Person"), 1u);
}

TEST(Index, EdgeCountWithType) {
  GraphDB db;
  NodeId a = db.create_node({}, {});
  NodeId b = db.create_node({}, {});
  db.create_edge(a, b, "KNOWS", {});
  db.create_edge(a, b, "KNOWS", {});
  db.create_edge(a, b, "LIKES", {});
  EXPECT_EQ(db.edge_count_with_type("KNOWS"), 2u);
  EXPECT_EQ(db.edge_count_with_type("LIKES"), 1u);
  EXPECT_EQ(db.edge_count_with_type("HATES"), 0u);
}

TEST(Index, PropertyDistinctCountNoLabel) {
  GraphDB db;
  db.create_node({}, make_props({{"color", Value{std::string("red")}}}));
  db.create_node({}, make_props({{"color", Value{std::string("blue")}}}));
  db.create_node({}, make_props({{"color", Value{std::string("red")}}}));
  auto cnt = db.property_distinct_count("color", "");
  ASSERT_TRUE(cnt.has_value());
  EXPECT_EQ(*cnt, 2u);
}

TEST(Index, PropertyDistinctCountWithLabel) {
  GraphDB db;
  db.create_node({"Car"}, make_props({{"color", Value{std::string("red")}}}));
  db.create_node({"Car"}, make_props({{"color", Value{std::string("blue")}}}));
  db.create_node({"Bike"}, make_props({{"color", Value{std::string("green")}}}));
  auto cnt = db.property_distinct_count("color", "Car");
  ASSERT_TRUE(cnt.has_value());
  EXPECT_EQ(*cnt, 2u);
}

TEST(Index, PropertyCount) {
  GraphDB db;
  db.create_node({"P"}, make_props({{"age", Value{int64_t(25)}}}));
  db.create_node({"P"}, make_props({{"age", Value{int64_t(25)}}}));
  db.create_node({"P"}, make_props({{"age", Value{int64_t(30)}}}));
  EXPECT_EQ(db.property_count("age", Value{int64_t(25)}, "P"), 2u);
  EXPECT_EQ(db.property_count("age", Value{int64_t(30)}, "P"), 1u);
  EXPECT_EQ(db.property_count("age", Value{int64_t(99)}, "P"), 0u);
}

TEST(Index, PropertyCountNoLabel) {
  GraphDB db;
  db.create_node({}, make_props({{"x", Value{int64_t(1)}}}));
  db.create_node({}, make_props({{"x", Value{int64_t(1)}}}));
  EXPECT_EQ(db.property_count("x", Value{int64_t(1)}, ""), 2u);
}

TEST(Index, HasPropertyIndex) {
  GraphDB db;
  db.create_node({"L"}, make_props({{"k", Value{int64_t(1)}}}));
  EXPECT_TRUE(db.has_property_index("L", "k"));
  EXPECT_FALSE(db.has_property_index("L", "missing"));
  EXPECT_FALSE(db.has_property_index("NoLabel", "k"));
}

TEST(Index, AvgOutDegree) {
  GraphDB db;
  NodeId a = db.create_node({"Hub"}, {});
  NodeId b = db.create_node({"Hub"}, {});
  NodeId c = db.create_node({}, {});
  db.create_edge(a, c, "T", {});
  db.create_edge(a, c, "T", {});
  db.create_edge(b, c, "T", {});
  EXPECT_DOUBLE_EQ(db.avg_out_degree("Hub"), 1.5);
}

TEST(Index, AvgOutDegreeNoLabel) {
  GraphDB db;
  EXPECT_DOUBLE_EQ(db.avg_out_degree("Ghost"), 0.0);
}

TEST(Index, AvgOutDegreeAfterDeleteNode) {
  GraphDB db;
  NodeId a = db.create_node({"Hub"}, {});
  NodeId b = db.create_node({}, {});
  db.create_edge(a, b, "T", {});
  db.delete_node(a);

  EXPECT_DOUBLE_EQ(db.avg_out_degree("Hub"), 0.0);
}

TEST(Cursor, AllNodes) {
  GraphDB db;
  db.create_node({}, {});
  db.create_node({}, {});
  db.create_node({}, {});
  auto cursor = db.all_nodes();
  int count = 0;
  Node* n;
  while (cursor->next(n)) ++count;
  EXPECT_EQ(count, 3);
}

TEST(Cursor, AllNodesSkipsDeleted) {
  GraphDB db;
  NodeId a = db.create_node({}, {});
  db.create_node({}, {});
  db.delete_node(a);
  auto cursor = db.all_nodes();
  int count = 0;
  Node* n;
  while (cursor->next(n)) ++count;
  EXPECT_EQ(count, 1);
}

TEST(Cursor, AllNodesWithLimit) {
  GraphDB db;
  for (int i = 0; i < 10; ++i) db.create_node({}, {});
  auto cursor = db.all_nodes(nullptr, 3);
  int count = 0;
  Node* n;
  while (cursor->next(n)) ++count;
  EXPECT_EQ(count, 3);
}

TEST(Cursor, AllNodesWithPredicate) {
  GraphDB db;
  db.create_node({}, make_props({{"active", Value{true}}}));
  db.create_node({}, make_props({{"active", Value{false}}}));
  db.create_node({}, make_props({{"active", Value{true}}}));
  auto cursor = db.all_nodes([](Node* n) {
    auto it = n->properties.find("active");
    return it != n->properties.end() && std::get<bool>(it->second);
  });
  int count = 0;
  Node* n;
  while (cursor->next(n)) ++count;
  EXPECT_EQ(count, 2);
}

TEST(Cursor, NodesWithLabel) {
  GraphDB db;
  db.create_node({"A"}, {});
  db.create_node({"A"}, {});
  db.create_node({"B"}, {});
  auto cursor = db.nodes_with_label("A");
  int count = 0;
  Node* n;
  while (cursor->next(n)) ++count;
  EXPECT_EQ(count, 2);
}

TEST(Cursor, NodesWithLabelMissing) {
  GraphDB db;
  auto cursor = db.nodes_with_label("Ghost");
  Node* n;
  EXPECT_FALSE(cursor->next(n));
}

TEST(Cursor, NodesWithProperty) {
  GraphDB db;
  db.create_node({}, make_props({{"role", Value{std::string("admin")}}}));
  db.create_node({}, make_props({{"role", Value{std::string("user")}}}));
  db.create_node({}, make_props({{"role", Value{std::string("admin")}}}));
  auto cursor = db.nodes_with_property("role", Value{std::string("admin")});
  int count = 0;
  Node* n;
  while (cursor->next(n)) ++count;
  EXPECT_EQ(count, 2);
}

TEST(Cursor, NodesWithPropertyMissing) {
  GraphDB db;
  auto cursor = db.nodes_with_property("no_key", Value{int64_t(0)});
  Node* n;
  EXPECT_FALSE(cursor->next(n));
}

TEST(Cursor, CursorReset) {
  GraphDB db;
  db.create_node({"X"}, {});
  db.create_node({"X"}, {});
  auto cursor = db.nodes_with_label("X");
  Node* n;
  int first = 0;
  while (cursor->next(n)) ++first;
  cursor->reset();
  int second = 0;
  while (cursor->next(n)) ++second;
  EXPECT_EQ(first, 2);
  EXPECT_EQ(second, 2);
}

TEST(Cursor, OutgoingEdges) {
  GraphDB db;
  NodeId a = db.create_node({}, {});
  NodeId b = db.create_node({}, {});
  NodeId c = db.create_node({}, {});
  db.create_edge(a, b, "T", {});
  db.create_edge(a, c, "T", {});
  auto cursor = db.outgoing_edges(a);
  int count = 0;
  Edge* e;
  while (cursor->next(e)) ++count;
  EXPECT_EQ(count, 2);
}

TEST(Cursor, OutgoingEdgesNoNode) {
  GraphDB db;
  auto cursor = db.outgoing_edges(999);
  Edge* e;
  EXPECT_FALSE(cursor->next(e));
}

TEST(Cursor, IncomingEdges) {
  GraphDB db;
  NodeId a = db.create_node({}, {});
  NodeId b = db.create_node({}, {});
  NodeId c = db.create_node({}, {});
  db.create_edge(a, c, "T", {});
  db.create_edge(b, c, "T", {});
  auto cursor = db.incoming_edges(c);
  int count = 0;
  Edge* e;
  while (cursor->next(e)) ++count;
  EXPECT_EQ(count, 2);
}

TEST(Cursor, EdgesByType) {
  GraphDB db;
  NodeId a = db.create_node({}, {});
  NodeId b = db.create_node({}, {});
  db.create_edge(a, b, "KNOWS", {});
  db.create_edge(a, b, "KNOWS", {});
  db.create_edge(a, b, "LIKES", {});
  auto cursor = db.edges_by_type("KNOWS");
  int count = 0;
  Edge* e;
  while (cursor->next(e)) ++count;
  EXPECT_EQ(count, 2);
}

TEST(Cursor, EdgesByTypeMissing) {
  GraphDB db;
  auto cursor = db.edges_by_type("GHOST");
  Edge* e;
  EXPECT_FALSE(cursor->next(e));
}

TEST(Cursor, OutgoingEdgesAfterDelete) {
  GraphDB db;
  NodeId a = db.create_node({}, {});
  NodeId b = db.create_node({}, {});
  EdgeId eid = db.create_edge(a, b, "T", {});
  db.delete_edge(eid);
  auto cursor = db.outgoing_edges(a);
  int count = 0;
  Edge* e;
  while (cursor->next(e)) ++count;
  EXPECT_EQ(count, 0);
}

TEST(Cursor, EdgeCursorWithPredicate) {
  GraphDB db;
  NodeId a = db.create_node({}, {});
  NodeId b = db.create_node({}, {});
  db.create_edge(a, b, "T", make_props({{"w", Value{int64_t(1)}}}));
  db.create_edge(a, b, "T", make_props({{"w", Value{int64_t(10)}}}));
  auto cursor = db.outgoing_edges(a, [](Edge* e) {
    auto it = e->properties.find("w");
    return it != e->properties.end() && std::get<int64_t>(it->second) > 5;
  });
  int count = 0;
  Edge* e;
  while (cursor->next(e)) ++count;
  EXPECT_EQ(count, 1);
}

class SaveLoad : public ::testing::Test {
protected:
  const std::string path = "/tmp/test_graphdb.bin";
  void TearDown() override { std::remove(path.c_str()); }
};

TEST_F(SaveLoad, RoundtripBasic) {
  GraphDB db;
  NodeId a = db.create_node({"Person"}, make_props({{"name", Value{std::string("Alice")}}}));
  NodeId b = db.create_node({"Person"}, make_props({{"name", Value{std::string("Bob")}}}));
  db.create_edge(a, b, "KNOWS", make_props({{"since", Value{int64_t(2020)}}}));

  ASSERT_TRUE(db.save_to_file(path));

  GraphDB db2;
  ASSERT_TRUE(db2.load_from_file(path));

  EXPECT_EQ(db2.node_count(), 2u);
  EXPECT_EQ(db2.edge_count_with_type("KNOWS"), 1u);

  Node* na = db2.get_node(a);
  ASSERT_NE(na, nullptr);
  EXPECT_EQ(std::get<std::string>(na->properties.at("name")), "Alice");
}

TEST_F(SaveLoad, RoundtripWithDeletedNodes) {
  GraphDB db;
  NodeId a = db.create_node({"X"}, {});
  db.create_node({"X"}, {});
  db.delete_node(a);

  ASSERT_TRUE(db.save_to_file(path));

  GraphDB db2;
  ASSERT_TRUE(db2.load_from_file(path));

  EXPECT_EQ(db2.node_count(), 1u);
  NodeId new_id = db2.create_node({}, {});
  EXPECT_EQ(new_id, a);
}

TEST_F(SaveLoad, AllValueTypes) {
  GraphDB db;
  db.create_node({}, make_props({
                                  {"i", Value{int64_t(-42)}},
                                  {"d", Value{3.14}},
                                  {"s", Value{std::string("hello")}},
                                  {"b", Value{false}}
                                }));

  ASSERT_TRUE(db.save_to_file(path));

  GraphDB db2;
  ASSERT_TRUE(db2.load_from_file(path));

  Node* n = db2.get_node(0);
  ASSERT_NE(n, nullptr);
  EXPECT_EQ(std::get<int64_t>(n->properties.at("i")), -42);
  EXPECT_DOUBLE_EQ(std::get<double>(n->properties.at("d")), 3.14);
  EXPECT_EQ(std::get<std::string>(n->properties.at("s")), "hello");
  EXPECT_EQ(std::get<bool>(n->properties.at("b")), false);
}

TEST_F(SaveLoad, LoadNonExistentFile) {
  GraphDB db;
  EXPECT_FALSE(db.load_from_file("/tmp/does_not_exist_xyz.bin"));
}

TEST_F(SaveLoad, EmptyGraphRoundtrip) {
  GraphDB db;
  ASSERT_TRUE(db.save_to_file(path));
  GraphDB db2;
  ASSERT_TRUE(db2.load_from_file(path));
  EXPECT_EQ(db2.node_count(), 0u);
}

TEST(EdgeCases, CreateManyNodesAndDelete) {
  GraphDB db;
  std::vector<NodeId> ids;
  for (int i = 0; i < 1000; ++i)
    ids.push_back(db.create_node({"Bulk"}, make_props({{"i", Value{int64_t(i)}}})));

  EXPECT_EQ(db.node_count(), 1000u);
  EXPECT_EQ(db.node_count_with_label("Bulk"), 1000u);

  for (int i = 0; i < 500; ++i)
    db.delete_node(ids[i]);

  EXPECT_EQ(db.node_count(), 500u);
  EXPECT_EQ(db.node_count_with_label("Bulk"), 500u);
}

TEST(EdgeCases, SetPropertyUpdatesIndex) {
  GraphDB db;
  NodeId id = db.create_node({}, make_props({{"k", Value{int64_t(1)}}}));
  EXPECT_EQ(db.property_count("k", Value{int64_t(1)}, ""), 1u);
  db.set_node_property(id, "k", Value{int64_t(2)});
  EXPECT_EQ(db.property_count("k", Value{int64_t(1)}, ""), 0u);
  EXPECT_EQ(db.property_count("k", Value{int64_t(2)}, ""), 1u);
}

TEST(EdgeCases, LimitZeroMeansUnlimited) {
  GraphDB db;
  for (int i = 0; i < 5; ++i) db.create_node({"T"}, {});
  auto cursor = db.nodes_with_label("T", nullptr, 0);
  int count = 0;
  Node* n;
  while (cursor->next(n)) ++count;
  EXPECT_EQ(count, 5);
}

TEST(EdgeCases, MultipleEdgesDeleteNode) {
  GraphDB db;
  NodeId a = db.create_node({}, {});
  NodeId b = db.create_node({}, {});
  NodeId c = db.create_node({}, {});
  EdgeId e1 = db.create_edge(a, b, "T", {});
  EdgeId e2 = db.create_edge(c, a, "T", {});
  db.delete_node(a);
  EXPECT_EQ(db.get_edge(e1), nullptr);
  EXPECT_EQ(db.get_edge(e2), nullptr);
  EXPECT_NE(db.get_node(b), nullptr);
  EXPECT_NE(db.get_node(c), nullptr);
}

TEST(EdgeCases, AvgOutDegreeAfterEdgeDelete) {
  GraphDB db;
  NodeId a = db.create_node({"Hub"}, {});
  NodeId b = db.create_node({}, {});
  EdgeId e1 = db.create_edge(a, b, "T", {});
  EdgeId e2 = db.create_edge(a, b, "T", {});
  EXPECT_DOUBLE_EQ(db.avg_out_degree("Hub"), 2.0);
  db.delete_edge(e1);
  EXPECT_DOUBLE_EQ(db.avg_out_degree("Hub"), 1.0);
  db.delete_edge(e2);
  EXPECT_DOUBLE_EQ(db.avg_out_degree("Hub"), 0.0);
}

TEST(EdgeCases, LabelIndexAfterDeleteLabel) {
  GraphDB db;
  NodeId id = db.create_node({"A"}, {});
  EXPECT_EQ(db.node_count_with_label("A"), 1u);
  db.delete_node_label(id, "A");
  EXPECT_EQ(db.node_count_with_label("A"), 0u);
}

TEST(EdgeCases, SaveLoadPreservesEdgeTypeIndex) {
  const std::string path = "/tmp/edge_type_test.bin";
  GraphDB db;
  NodeId a = db.create_node({}, {});
  NodeId b = db.create_node({}, {});
  db.create_edge(a, b, "FOO", {});
  db.create_edge(a, b, "FOO", {});
  db.save_to_file(path);

  GraphDB db2;
  db2.load_from_file(path);
  EXPECT_EQ(db2.edge_count_with_type("FOO"), 2u);

  auto cursor = db2.outgoing_edges(a);
  int count = 0;
  Edge* e;
  while (cursor->next(e)) ++count;
  EXPECT_EQ(count, 2);

  std::remove(path.c_str());
}