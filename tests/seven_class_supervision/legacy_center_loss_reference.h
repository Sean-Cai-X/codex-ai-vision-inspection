// Test-only frozen reference from f89ce6a, torch_runtime_yolov8_seg_executor.cpp.
// Function body is verbatim; only its symbol name differs. Never used by production.
BusinessLossTensors LegacyBusinessYoloV8SegLoss(
    YoloV8Segment& model,
    const YoloV8SegRawOutput& raw,
    const YoloV8SegDatasetSample& sample,
    const torch::Tensor& input,
    const SegLetterbox& letterbox,
    const TorchModelManifest& manifest)
{
    BusinessLossTensors result;
    const auto options = input.options();
    result.class_loss = torch::zeros({}, options);
    result.mask_loss = torch::zeros({}, options);
    result.box_loss = torch::zeros({}, options);
    result.dfl_loss = torch::zeros({}, options);
    const int proto_height = static_cast<int>(raw.prototypes.size(2));
    const int proto_width = static_cast<int>(raw.prototypes.size(3));
    const torch::Tensor proto_flat =
        raw.prototypes.index({0}).view({manifest.mask_channels, -1});
    const std::size_t level_count = raw.class_logits.size();
    for (std::size_t target_index = 0;
         target_index < sample.classes.size(); ++target_index)
    {
        const auto& box = sample.boxes_xyxy_norm[target_index];
        const auto to_input_x = [&](float value) {
            return static_cast<float>((letterbox.pad_x +
                value * letterbox.resized_width) /
                static_cast<double>(manifest.input_width));
        };
        const auto to_input_y = [&](float value) {
            return static_cast<float>((letterbox.pad_y +
                value * letterbox.resized_height) /
                static_cast<double>(manifest.input_height));
        };
        const std::array<float, 4> input_box{
            to_input_x(box[0]), to_input_y(box[1]),
            to_input_x(box[2]), to_input_y(box[3])};
        const int64_t class_id = sample.classes[target_index];
        TORCH_CHECK(class_id >= 0 && class_id < manifest.num_classes,
            "business dataset class ID is outside the fixed head");
        const torch::Tensor mask_target = BusinessMaskTarget(
            sample.polygons_norm[target_index], proto_width, proto_height,
            letterbox, manifest, options);
        const float center_x =
            (input_box[0] + input_box[2]) * 0.5f;
        const float center_y =
            (input_box[1] + input_box[3]) * 0.5f;
        for (std::size_t level = 0; level < level_count; ++level)
        {
            const int64_t feature_height = raw.class_logits[level].size(2);
            const int64_t feature_width = raw.class_logits[level].size(3);
            const int64_t column = std::clamp<int64_t>(
                static_cast<int64_t>(std::floor(center_x * feature_width)),
                0, feature_width - 1);
            const int64_t row = std::clamp<int64_t>(
                static_cast<int64_t>(std::floor(center_y * feature_height)),
                0, feature_height - 1);
            const torch::Tensor class_logits = raw.class_logits[level].index(
                {0, torch::indexing::Slice(), row, column});
            torch::Tensor class_target = torch::zeros_like(class_logits);
            class_target.index_put_({class_id}, 1.0);
            result.class_loss = result.class_loss +
                torch::binary_cross_entropy_with_logits(
                    class_logits, class_target);

            const torch::Tensor coefficients =
                raw.mask_coefficients[level].index(
                    {0, torch::indexing::Slice(), row, column});
            const torch::Tensor mask_logits =
                torch::matmul(coefficients, proto_flat)
                    .view({proto_height, proto_width});
            const torch::Tensor mask_bce =
                torch::binary_cross_entropy_with_logits(
                    mask_logits, mask_target);
            const torch::Tensor mask_probability = mask_logits.sigmoid();
            const torch::Tensor dice = 1.0 -
                (2.0 * (mask_probability * mask_target).sum() + 1.0) /
                (mask_probability.sum() + mask_target.sum() + 1.0);
            result.mask_loss = result.mask_loss + mask_bce + dice;

            const float stride =
                static_cast<float>(manifest.input_width) /
                static_cast<float>(feature_width);
            const float anchor_x =
                (static_cast<float>(column) + 0.5f) * stride;
            const float anchor_y =
                (static_cast<float>(row) + 0.5f) * stride;
            const torch::Tensor target_distances = torch::tensor(
                {std::max(0.0f,
                     (anchor_x - input_box[0] * manifest.input_width) / stride),
                 std::max(0.0f,
                     (anchor_y - input_box[1] * manifest.input_height) / stride),
                 std::max(0.0f,
                     (input_box[2] * manifest.input_width - anchor_x) / stride),
                 std::max(0.0f,
                     (input_box[3] * manifest.input_height - anchor_y) / stride)},
                options).clamp(0.0, 15.0 - 1.0e-3);
            const torch::Tensor box_logits = raw.box_logits[level]
                .index({0, torch::indexing::Slice(), row, column})
                .view({1, 64, 1});
            const torch::Tensor predicted_distances =
                model->head()->dfl_module()->expectation(box_logits)
                    .view({4});
            result.box_loss = result.box_loss +
                torch::abs(predicted_distances - target_distances).mean();
            const torch::Tensor dfl_logits = box_logits.view({4, 16});
            const torch::Tensor target_left =
                torch::floor(target_distances).to(torch::kLong);
            const torch::Tensor target_right =
                (target_left + 1).clamp_max(15);
            const torch::Tensor right_weight =
                (target_distances -
                 target_left.to(target_distances.dtype()))
                    .clamp(0.0, 1.0);
            const torch::Tensor left_weight = 1.0 - right_weight;
            const torch::Tensor left_loss =
                torch::nn::functional::cross_entropy(
                    dfl_logits, target_left,
                    torch::nn::functional::CrossEntropyFuncOptions()
                        .reduction(torch::kNone));
            const torch::Tensor right_loss =
                torch::nn::functional::cross_entropy(
                    dfl_logits, target_right,
                    torch::nn::functional::CrossEntropyFuncOptions()
                        .reduction(torch::kNone));
            result.dfl_loss = result.dfl_loss +
                (left_loss * left_weight + right_loss * right_weight).mean();
        }
    }
    const double divisor = static_cast<double>(
        std::max<std::size_t>(1u, sample.classes.size() * level_count));
    result.class_loss = result.class_loss / divisor;
    result.mask_loss = result.mask_loss / divisor;
    result.box_loss = result.box_loss / divisor;
    result.dfl_loss = result.dfl_loss / divisor;
    result.total_loss =
        result.class_loss + result.mask_loss +
        result.box_loss * 0.25 + result.dfl_loss * 0.25;
    return result;
}
