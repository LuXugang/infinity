//
// Created by JinHai on 2022/10/30.
//

#include "common/types/internal_types.h"
#include "common/types/geo/box_type.h"
#include "common/utility/infinity_assert.h"
#include "main/stats/global_resource_usage.h"

namespace infinity {

struct PolygonType {
public:
    ptr_t ptr {};
    i32 point_count {0}; // 65535 point are the max point count
    BoxType bounding_box {};

public:

    PolygonType() = default;
    inline ~PolygonType() {
        Reset();
    }

    explicit inline
    PolygonType(i32 count): point_count(count) {
        if(count == 0) return ;

        ptr = new char_t[point_count * sizeof(PointType)]{0};
        GlobalResourceUsage::IncrRawMemCount();

        ResetBoundingBox();
    }

    PolygonType(const PolygonType& other);
    PolygonType(PolygonType&& other) noexcept;
    PolygonType& operator=(const PolygonType& other);
    PolygonType& operator=(PolygonType&& other) noexcept;

    inline void
    SetPoint(i32 index, PointType point) {
        if(ptr == nullptr) TypeError("Not initialized.");
        if(index >= point_count) TypeError("Index is larger than point count");
        ((PointType*)(ptr))[index] = point;
        bounding_box.upper_left.x = std::min(bounding_box.upper_left.x, point.x);
        bounding_box.upper_left.y = std::max(bounding_box.upper_left.y, point.y);
        bounding_box.lower_right.x = std::max(bounding_box.lower_right.x, point.x);
        bounding_box.lower_right.y = std::min(bounding_box.lower_right.y, point.y);
    }

    inline PointType
    GetPoint(i32 index) {
        if(ptr == nullptr) TypeError("Not initialized.");
        return ((PointType*)(ptr))[index];
    }

    [[nodiscard]] inline i32
    PointCount() const {
        return point_count;
    }

    inline void
    Initialize(i32 count, i32 is_closed = 0) {
        if(point_count != 0) {
            TypeError("Already initialized, need to reset before re-initialize");
        }
        if(count == 0) return ;
        point_count = count;

        ptr = new char_t[point_count * sizeof(PointType)]{0};
        GlobalResourceUsage::IncrRawMemCount();

        ResetBoundingBox();
    }

    inline void
    Reset() {
        if(point_count == 0) return;
        point_count = 0;
        ResetBoundingBox();

        delete[] ptr;
        GlobalResourceUsage::DecrRawMemCount();
    }

    inline void
    ResetBoundingBox() {
        bounding_box.upper_left.x = std::numeric_limits<f64>::max();
        bounding_box.upper_left.y = std::numeric_limits<f64>::min();
        bounding_box.lower_right.x = std::numeric_limits<f64>::min();
        bounding_box.lower_right.y = std::numeric_limits<f64>::max();
    }
};

}

