//
// Created by jinhai on 23-3-10.
//

#include <gtest/gtest.h>
#include "base_test.h"
#include "common/column_vector/column_vector.h"
#include "common/types/value.h"
#include "main/logger.h"
#include "main/stats/global_resource_usage.h"
#include "main/infinity.h"
#include "function/cast/varchar_cast.h"
#include "common/types/info/varchar_info.h"

class VarcharTest : public BaseTest {
    void
    SetUp() override {
        infinity::GlobalResourceUsage::Init();
        infinity::Infinity::instance().Init();
    }

    void
    TearDown() override {
        infinity::Infinity::instance().UnInit();
        EXPECT_EQ(infinity::GlobalResourceUsage::GetObjectCount(), 0);
        EXPECT_EQ(infinity::GlobalResourceUsage::GetRawMemoryCount(), 0);
        infinity::GlobalResourceUsage::UnInit();
    }
};

TEST_F(VarcharTest, varchar_cast0) {
    using namespace infinity;
    {
        auto varchar_info = VarcharInfo::Make(65);
        DataType source_type(LogicalType::kVarchar, varchar_info);
        String s1 = "true";

        {
            DataType target_type(LogicalType::kBoolean);

            VarcharType v1;
            v1.Initialize(s1);
            EXPECT_EQ(v1.length, s1.length());
            EXPECT_EQ(v1.ToString(), s1);

            BooleanT target{false};
            TryCastVarchar::Run(v1, target);
            EXPECT_TRUE(target);
        }

        s1 = "false";
        {
            DataType target_type(LogicalType::kBoolean);

            VarcharType v1;
            v1.Initialize(s1);
            EXPECT_EQ(v1.length, s1.length());
            EXPECT_EQ(v1.ToString(), s1);

            BooleanT target{true};
            TryCastVarchar::Run(v1, target);
            EXPECT_FALSE(target);
        }
    }
}