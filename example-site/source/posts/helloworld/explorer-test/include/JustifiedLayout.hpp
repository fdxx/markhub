#pragma once

#include <vector>
#include <variant>
#include <optional>
#include <stdexcept>
#include <string>
#include <algorithm>
#include <cmath>
#include <limits>

namespace JustifiedLayout {

// ─────────────────────────────────────────────
// Config
// ─────────────────────────────────────────────

struct Padding {
    double top    = 10;
    double right  = 10;
    double bottom = 10;
    double left   = 10;
};

struct Spacing {
    double horizontal = 10;
    double vertical   = 10;
};

enum class WidowLayoutStyle { Left, Center, Justify };

struct LayoutConfig {
    double            containerWidth             = 1060;
    Padding           containerPadding           = {};
    Spacing           boxSpacing                 = {};
    double            targetRowHeight            = 320;
    double            targetRowHeightTolerance   = 0.25;
    std::size_t       maxNumRows                 = std::numeric_limits<std::size_t>::max();
    std::optional<double> forceAspectRatio       = std::nullopt;
    bool              showWidows                 = true;
    std::optional<std::size_t> fullWidthBreakoutRowCadence = std::nullopt;
    WidowLayoutStyle  widowLayoutStyle           = WidowLayoutStyle::Left;
};

// ─────────────────────────────────────────────
// Input
// ─────────────────────────────────────────────

struct ItemSize {
    double width;
    double height;
};

using InputItem = std::variant<ItemSize, double>;

// ─────────────────────────────────────────────
// Output
// ─────────────────────────────────────────────

struct Box {
    double top    = 0;
    double left   = 0;
    double width  = 0;
    double height = 0;

    double aspectRatio       = 1;
    bool   forcedAspectRatio = false;
};

struct LayoutResult {
    double            containerHeight = 0;
    std::size_t       widowCount      = 0;
    std::vector<Box>  boxes;
};

// ─────────────────────────────────────────────
// Internal: Row
// ─────────────────────────────────────────────

class Row {
public:
    struct Params {
        double           top;
        double           left;
        double           width;
        double           spacing;
        double           targetRowHeight;
        double           targetRowHeightTolerance;
        double           edgeCaseMinRowHeight;
        double           edgeCaseMaxRowHeight;
        bool             isBreakoutRow;
        WidowLayoutStyle widowLayoutStyle;
    };

    explicit Row(const Params& p)
        : top_(p.top)
        , left_(p.left)
        , width_(p.width)
        , spacing_(p.spacing)
        , targetRowHeight_(p.targetRowHeight)
        , minAspectRatio_(p.width / p.targetRowHeight * (1.0 - p.targetRowHeightTolerance))
        , maxAspectRatio_(p.width / p.targetRowHeight * (1.0 + p.targetRowHeightTolerance))
        , edgeCaseMinRowHeight_(p.edgeCaseMinRowHeight)
        , edgeCaseMaxRowHeight_(p.edgeCaseMaxRowHeight)
        , widowLayoutStyle_(p.widowLayoutStyle)
        , isBreakoutRow_(p.isBreakoutRow)
        , height_(0)
        // ❶ 预分配：典型行不超过 16 张图
        , aspectRatioSum_(0.0)
    {
        items_.reserve(16);
    }

    // 返回 true 表示 item 已入行，false 表示被拒绝（调用方重试新行）
    bool addItem(Box item) {
        const double newSum   = aspectRatioSum_ + item.aspectRatio;
        const std::size_t newCount = items_.size() + 1;
        const double newRowWidthWithoutSpacing =
            width_ - static_cast<double>(newCount - 1) * spacing_;
        const double targetAspectRatio = newRowWidthWithoutSpacing / targetRowHeight_;

        // ── Breakout 行：单张横向/方形图铺满整行 ──
        if (isBreakoutRow_ && items_.empty() && item.aspectRatio >= 1.0) {
            items_.push_back(item);
            aspectRatioSum_ = item.aspectRatio;
            completeLayout(newRowWidthWithoutSpacing / item.aspectRatio,
                           WidowLayoutStyle::Justify);
            return true;
        }

        if (newSum < minAspectRatio_) {
            // 行高仍然过高，继续收集
            items_.push_back(item);
            aspectRatioSum_ = newSum;
            return true;
        }

        if (newSum > maxAspectRatio_) {
            // 行高会过矮
            if (items_.empty()) {
                // 超宽单图：强制接受
                items_.push_back(item);
                aspectRatioSum_ = newSum;
                completeLayout(newRowWidthWithoutSpacing / newSum,
                               WidowLayoutStyle::Justify);
                return true;
            }

            // ❷ 使用预维护的 aspectRatioSum_ 避免重复 accumulate
            const double prevRowWidthWithoutSpacing =
                width_ - static_cast<double>(items_.size() - 1) * spacing_;
            const double prevTargetAspectRatio = prevRowWidthWithoutSpacing / targetRowHeight_;

            if (std::abs(newSum  - targetAspectRatio) >
                std::abs(aspectRatioSum_ - prevTargetAspectRatio)) {
                // 旧行更接近目标，拒绝新 item
                completeLayout(prevRowWidthWithoutSpacing / aspectRatioSum_,
                               WidowLayoutStyle::Justify);
                return false;
            } else {
                // 新行更接近目标，接受
                items_.push_back(item);
                aspectRatioSum_ = newSum;
                completeLayout(newRowWidthWithoutSpacing / newSum,
                               WidowLayoutStyle::Justify);
                return true;
            }
        }

        // 在容差范围内，接受并关闭行
        items_.push_back(item);
        aspectRatioSum_ = newSum;
        completeLayout(newRowWidthWithoutSpacing / newSum,
                       WidowLayoutStyle::Justify);
        return true;
    }

    [[nodiscard]] bool isLayoutComplete() const { return height_ > 0; }

    void forceComplete(bool /*fitToWidth*/ = false,
                       std::optional<double> rowHeight = std::nullopt) {
        completeLayout(rowHeight.value_or(targetRowHeight_), widowLayoutStyle_);
    }

    [[nodiscard]] const std::vector<Box>& getItems() const { return items_; }
    [[nodiscard]] double height()          const { return height_; }
    [[nodiscard]] double targetRowHeight() const { return targetRowHeight_; }
    [[nodiscard]] bool   isBreakoutRow()   const { return isBreakoutRow_; }

private:
    void completeLayout(double newHeight, WidowLayoutStyle style) {
        const std::size_t n = items_.size();

        // 限幅到边缘高度范围
        const double clampedHeight =
            std::clamp(newHeight, edgeCaseMinRowHeight_, edgeCaseMaxRowHeight_);

        double widthScale;
        if (newHeight != clampedHeight) {
            height_ = clampedHeight;
            // ❸ 合并两次除法为一次乘法
            widthScale = newHeight / clampedHeight;
        } else {
            height_ = newHeight;
            widthScale = 1.0;
        }

        // 第一遍：按比例分配宽度并从左排列
        double cursor = left_;
        for (auto& item : items_) {
            item.top    = top_;
            item.height = height_;
            item.width  = item.aspectRatio * height_ * widthScale;
            item.left   = cursor;
            cursor += item.width + spacing_;
        }

        if (style == WidowLayoutStyle::Justify) {
            // ❹ 消除重复的 itemWidthSum 累加，直接复用 cursor
            const double totalUsed   = cursor - spacing_ - left_;
            const double errorPerItem = (totalUsed - width_) / static_cast<double>(n);

            if (n == 1) {
                items_[0].width -= std::round(errorPerItem);
            } else {
                // ❺ 将 (i+1)*errorPerItem 转为增量更新，只调用一次 std::round
                double prevCumError = 0.0;
                for (std::size_t i = 0; i < n; ++i) {
                    const double cumError = std::round(static_cast<double>(i + 1) * errorPerItem);
                    const double delta    = cumError - prevCumError;
                    items_[i].left  -= prevCumError;
                    items_[i].width -= delta;
                    prevCumError     = cumError;
                }
            }
        } else if (style == WidowLayoutStyle::Center) {
            const double totalUsed  = cursor - spacing_ - left_;
            const double centerOffset = (width_ - totalUsed) / 2.0;
            for (auto& item : items_) {
                item.left += centerOffset + spacing_;
            }
        }
        // WidowLayoutStyle::Left：无需额外处理
    }

    double           top_;
    double           left_;
    double           width_;
    double           spacing_;
    double           targetRowHeight_;
    double           minAspectRatio_;
    double           maxAspectRatio_;
    double           edgeCaseMinRowHeight_;
    double           edgeCaseMaxRowHeight_;
    WidowLayoutStyle widowLayoutStyle_;
    bool             isBreakoutRow_;
    double           height_;
    std::vector<Box> items_;
    double           aspectRatioSum_;   // ❷ 增量维护，避免 O(n) accumulate
};

// ─────────────────────────────────────────────
// Internal: layout state
// ─────────────────────────────────────────────

struct LayoutData {
    std::vector<Box>  layoutItems;
    double            containerHeight = 0;
    std::vector<Row>  rows;
};

// ─────────────────────────────────────────────
// Internal helpers
// ─────────────────────────────────────────────

inline Row createNewRow(const LayoutConfig& cfg, const LayoutData& data) {
    const bool isBreakout = [&]() -> bool {
        if (!cfg.fullWidthBreakoutRowCadence.has_value()) return false;
        const std::size_t cadence = *cfg.fullWidthBreakoutRowCadence;
        return cadence > 0 && ((data.rows.size() + 1) % cadence) == 0;
    }();

    return Row({
        .top                      = data.containerHeight,
        .left                     = cfg.containerPadding.left,
        .width                    = cfg.containerWidth
                                    - cfg.containerPadding.left
                                    - cfg.containerPadding.right,
        .spacing                  = cfg.boxSpacing.horizontal,
        .targetRowHeight          = cfg.targetRowHeight,
        .targetRowHeightTolerance = cfg.targetRowHeightTolerance,
        .edgeCaseMinRowHeight     = 0.5 * cfg.targetRowHeight,
        .edgeCaseMaxRowHeight     = 2.0 * cfg.targetRowHeight,
        .isBreakoutRow            = isBreakout,
        .widowLayoutStyle         = cfg.widowLayoutStyle,
    });
}

// ❻ 接受右值引用，避免额外拷贝；直接用 append_range / insert 批量追加
inline void addRow(const LayoutConfig& cfg, LayoutData& data, Row& row) {
    const auto& items = row.getItems();
    data.layoutItems.insert(data.layoutItems.end(), items.begin(), items.end());
    data.rows.push_back(std::move(row));
    data.containerHeight += data.rows.back().height() + cfg.boxSpacing.vertical;
}

// ─────────────────────────────────────────────
// computeLayout
// ─────────────────────────────────────────────

inline LayoutResult computeLayout(const LayoutConfig&    cfg,
                                  LayoutData&            data,
                                  const std::vector<Box>& itemLayoutData) {
    std::size_t widowCount = 0;
    const std::size_t total = itemLayoutData.size();

    // ❼ 用 std::optional<Row> + in-place reset 替代反复构造，减少堆分配
    //    同时，两处"新行立即 addItem 可能立即 complete"的逻辑合并为循环
    std::optional<Row> currentRow;
    bool done = false;

    auto ensureRow = [&]() {
        if (!currentRow.has_value())
            currentRow.emplace(createNewRow(cfg, data));
    };

    auto flushRow = [&]() -> bool {
        addRow(cfg, data, *currentRow);
        currentRow.reset();
        if (data.rows.size() >= cfg.maxNumRows) {
            done = true;
            return false;
        }
        return true;
    };

    for (std::size_t i = 0; i < total && !done; ++i) {
        const Box& itemData = itemLayoutData[i];

        if (std::isnan(itemData.aspectRatio)) [[unlikely]] {
            throw std::invalid_argument(
                "Item " + std::to_string(i) + " has an invalid aspect ratio");
        }

        ensureRow();

        bool itemAdded = currentRow->addItem(itemData);

        if (currentRow->isLayoutComplete()) {
            if (!flushRow()) break;
            ensureRow();

            if (!itemAdded) {
                itemAdded = currentRow->addItem(itemData);
                if (currentRow->isLayoutComplete()) {
                    if (!flushRow()) break;
                    ensureRow();
                }
            }
        }
    }

    // 处理孤行 (widows)
    if (!done && currentRow.has_value() && !currentRow->getItems().empty()
        && cfg.showWidows) {

        std::optional<double> refHeight;
        if (!data.rows.empty()) {
            const Row& prevRow = data.rows.back();
            refHeight = prevRow.isBreakoutRow()
                ? prevRow.targetRowHeight()
                : prevRow.height();
        }
        currentRow->forceComplete(false, refHeight);
        widowCount = currentRow->getItems().size();
        addRow(cfg, data, *currentRow);
        currentRow.reset();
    }

    data.containerHeight -= cfg.boxSpacing.vertical;
    data.containerHeight += cfg.containerPadding.bottom;

    return LayoutResult{
        .containerHeight = data.containerHeight,
        .widowCount      = widowCount,
        .boxes           = std::move(data.layoutItems),   // ❽ move 而非拷贝
    };
}

// ─────────────────────────────────────────────
// Public entry point
// ─────────────────────────────────────────────

inline LayoutResult compute(const std::vector<InputItem>& input,
                            LayoutConfig config = {}) {
    LayoutData data;
    data.containerHeight = config.containerPadding.top;

    // ❾ 预分配输出容器和行列表
    const std::size_t n = input.size();
    data.layoutItems.reserve(n);

    // 粗估行数（实际行数 ≤ 图片数，一般远小于）
    const double approxItemsPerRow =
        (config.containerWidth - config.containerPadding.left - config.containerPadding.right)
        / config.targetRowHeight;
    if (approxItemsPerRow > 0)
        data.rows.reserve(static_cast<std::size_t>(n / approxItemsPerRow) + 2);

    std::vector<Box> items;
    items.reserve(n);

    const bool forceAR = config.forceAspectRatio.has_value();
    const double forcedAR = forceAR ? *config.forceAspectRatio : 0.0;

    for (const auto& in : input) {
        Box box;
        if (forceAR) {
            box.aspectRatio       = forcedAR;
            box.forcedAspectRatio = true;
        } else if (std::holds_alternative<ItemSize>(in)) {
            const auto& sz = std::get<ItemSize>(in);
            box.aspectRatio = sz.width / sz.height;
        } else {
            box.aspectRatio = std::get<double>(in);
        }
        items.push_back(box);
    }

    return computeLayout(config, data, items);
}

} // namespace JustifiedLayout
