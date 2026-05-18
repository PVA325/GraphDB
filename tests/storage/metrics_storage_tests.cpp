#include <filesystem>
#include <gtest/gtest.h>

#include "storage/metrics_store.hpp"
#include "storage/TypesStructures/types.hpp"

namespace storage {
  namespace {

    const std::filesystem::path kTestDir = "/tmp/metrics_test";

    struct MetricsTest : ::testing::Test {
      void SetUp() override {
        std::filesystem::remove_all(kTestDir);
        std::filesystem::create_directories(kTestDir);
      }
    };


    TEST_F(MetricsTest, NodeCount) {
      MetricsStore ms("");
      EXPECT_EQ(ms.node_count(), 0u);
      ms.on_node_created({"Person"}, {{"age", Value{int64_t(25)}}});
      EXPECT_EQ(ms.node_count(), 1u);
      ms.on_node_created({"Person"}, {{"age", Value{int64_t(30)}}});
      EXPECT_EQ(ms.node_count(), 2u);
      ms.on_node_deleted({"Person"}, {{"age", Value{int64_t(25)}}});
      EXPECT_EQ(ms.node_count(), 1u);
    }

    TEST_F(MetricsTest, NodeCountWithLabel) {
      MetricsStore ms("");
      ms.on_node_created({"Person"}, {});
      ms.on_node_created({"Person"}, {});
      ms.on_node_created({"Car"}, {});
      EXPECT_EQ(ms.node_count_with_label("Person"), 2u);
      EXPECT_EQ(ms.node_count_with_label("Car"), 1u);
      EXPECT_EQ(ms.node_count_with_label("Ghost"), 0u);
    }

    TEST_F(MetricsTest, AvgOutDegree) {
      MetricsStore ms("");
      ms.on_node_created({"Person"}, {});
      ms.on_node_created({"Person"}, {});
      ms.on_edge_created({"Person"});
      ms.on_edge_created({"Person"});
      ms.on_edge_created({"Person"});
      EXPECT_DOUBLE_EQ(ms.avg_out_degree("Person"), 1.5);
    }

    TEST_F(MetricsTest, AvgOutDegreeNoNodes) {
      MetricsStore ms("");
      EXPECT_DOUBLE_EQ(ms.avg_out_degree("Person"), 0.0);
    }

    TEST_F(MetricsTest, LabelAdded) {
      MetricsStore ms("");
      ms.on_node_created({"Person"}, {});
      EXPECT_EQ(ms.node_count_with_label("Person"), 1u);
      EXPECT_EQ(ms.node_count_with_label("Employee"), 0u);
      ms.on_label_added("Employee", {}, 2);
      EXPECT_EQ(ms.node_count_with_label("Employee"), 1u);
      EXPECT_DOUBLE_EQ(ms.avg_out_degree("Employee"), 2.0);
    }

    TEST_F(MetricsTest, LabelRemoved) {
      MetricsStore ms("");
      ms.on_node_created({"Person"}, {});
      ms.on_label_added("Employee", {}, 3);
      ms.on_label_removed("Employee", 3);
      EXPECT_EQ(ms.node_count_with_label("Employee"), 0u);
      EXPECT_DOUBLE_EQ(ms.avg_out_degree("Employee"), 0.0);
    }

    TEST_F(MetricsTest, EdgeDeletedUpdatesOutDegree) {
      MetricsStore ms("");
      ms.on_node_created({"Person"}, {});
      ms.on_edge_created({"Person"});
      ms.on_edge_created({"Person"});
      ms.on_edge_deleted({"Person"});
      EXPECT_DOUBLE_EQ(ms.avg_out_degree("Person"), 1.0);
    }

    TEST_F(MetricsTest, Clear) {
      MetricsStore ms("");
      ms.on_node_created({"Person"}, {{"age", Value{int64_t(25)}}});
      ms.on_edge_created({"Person"});
      ms.clear();
      EXPECT_EQ(ms.node_count(), 0u);
      EXPECT_EQ(ms.node_count_with_label("Person"), 0u);
      EXPECT_DOUBLE_EQ(ms.avg_out_degree("Person"), 0.0);
    }


    TEST_F(MetricsTest, PersistAndLoad) {
      {
        MetricsStore ms(kTestDir);
        ms.on_node_created({"Person"}, {{"age", Value{int64_t(25)}}});
        ms.on_node_created({"Person"}, {{"age", Value{int64_t(30)}}});
        ms.on_node_created({"Car"}, {});
        ms.on_edge_created({"Person"});
        ms.flush();
      }
      MetricsStore ms2(kTestDir);
      EXPECT_EQ(ms2.node_count(), 3u);
      EXPECT_EQ(ms2.node_count_with_label("Person"), 2u);
      EXPECT_EQ(ms2.node_count_with_label("Car"), 1u);
      EXPECT_DOUBLE_EQ(ms2.avg_out_degree("Person"), 0.5);
    }

    TEST_F(MetricsTest, DeltaLogReplayedOnLoad) {
      {
        MetricsStore ms(kTestDir);
        ms.on_node_created({"Person"}, {});
      }

      MetricsStore ms2(kTestDir);
      EXPECT_EQ(ms2.node_count(), 1u);
      EXPECT_EQ(ms2.node_count_with_label("Person"), 1u);
    }

    TEST_F(MetricsTest, SnapshotAndDeltaTogether) {
      {
        MetricsStore ms(kTestDir);
        ms.on_node_created({"Person"}, {});
        ms.flush();
        ms.on_node_created({"Car"}, {});
      }
      MetricsStore ms2(kTestDir);
      EXPECT_EQ(ms2.node_count(), 2u);
      EXPECT_EQ(ms2.node_count_with_label("Person"), 1u);
      EXPECT_EQ(ms2.node_count_with_label("Car"), 1u);
    }

    TEST_F(MetricsTest, MultipleFlushes) {
      {
        MetricsStore ms(kTestDir);
        ms.on_node_created({"A"}, {});
        ms.flush();
        ms.on_node_created({"B"}, {});
        ms.flush();
        ms.on_node_created({"C"}, {});
        ms.flush();
      }
      MetricsStore ms2(kTestDir);
      EXPECT_EQ(ms2.node_count(), 3u);
    }


    TEST_F(MetricsTest, LoadFromEmptyDir) {
      MetricsStore ms(kTestDir);
      EXPECT_EQ(ms.node_count(), 0u);
    }

    TEST_F(MetricsTest, LoadNoDir) {
      MetricsStore ms("");
      ms.on_node_created({"Person"}, {});
      ms.flush();
      EXPECT_EQ(ms.node_count(), 1u);
    }

    TEST_F(MetricsTest, DeleteMoreThanCreated) {
      MetricsStore ms("");
      ms.on_node_created({"Person"}, {});
      ms.on_node_deleted({"Person"}, {});
      EXPECT_EQ(ms.node_count(), 0u);
      EXPECT_EQ(ms.node_count_with_label("Person"), 0u);
    }

    TEST_F(MetricsTest, EdgeDeletedMoreThanCreated) {
      MetricsStore ms("");
      ms.on_node_created({"Person"}, {});
      ms.on_edge_created({"Person"});
      ms.on_edge_deleted({"Person"});
      ms.on_edge_deleted({"Person"});
      EXPECT_DOUBLE_EQ(ms.avg_out_degree("Person"), 0.0);
    }

    TEST_F(MetricsTest, LabelRemovedOutDegreeUnderflow) {
      MetricsStore ms("");
      ms.on_node_created({"Person"}, {});
      ms.on_label_added("Person", {}, 1);
      ms.on_label_removed("Person", 999);
      EXPECT_DOUBLE_EQ(ms.avg_out_degree("Person"), 1.0);
    }

    TEST_F(MetricsTest, UnknownLabelQueries) {
      MetricsStore ms("");
      EXPECT_EQ(ms.node_count_with_label("Ghost"), 0u);
      EXPECT_DOUBLE_EQ(ms.avg_out_degree("Ghost"), 0.0);
    }

    TEST_F(MetricsTest, MaybeFlushTriggered) {
      MetricsStore ms(kTestDir);
      for (size_t i = 0; i < MetricsStore::kMaxMetricPageAmount; ++i) {
        ms.on_node_created({"Person"}, {});
      }
      EXPECT_TRUE(std::filesystem::exists(kTestDir / "metrics_delta.log"));
      EXPECT_EQ(ms.node_count(), MetricsStore::kMaxMetricPageAmount);
    }

  } // namespace
} // namespace storage