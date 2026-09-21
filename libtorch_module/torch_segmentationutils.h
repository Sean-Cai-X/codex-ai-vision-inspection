
#include <opencv2/opencv.hpp>
#include "torch_deeplabv3_plus.h"

class SegmentationUtils {
public:
    static cv::Mat tensor_to_mask(const torch::Tensor& output_tensor) {

        auto pred = output_tensor.argmax(1).squeeze(0).cpu();
        pred = pred.to(torch::kByte);

        int h = pred.size(0);
        int w = pred.size(1);

        cv::Mat mask(h, w, CV_8UC1, pred.data_ptr<uint8_t>());

        cv::Mat color_mask;
        cv::applyColorMap(mask * 10, color_mask, cv::COLORMAP_JET);
        return color_mask;
    }

    static cv::Mat extract_binary_mask(const torch::Tensor& output_tensor, int target_class_id) {
        auto pred = output_tensor.argmax(1).squeeze(0).cpu();

        auto binary_tensor = (pred == target_class_id).to(torch::kByte) * 255;

        int h = binary_tensor.size(0);
        int w = binary_tensor.size(1);

        cv::Mat bin_mask(h, w, CV_8UC1);
        std::memcpy(bin_mask.data, binary_tensor.data_ptr<uint8_t>(), h * w);

        return bin_mask;
    }
};

void run_segmentation_inference() {
    std::cout << "Loading DeepLabV3+ with MobileNetV3..." << std::endl;

    int num_classes = 21;
    DeepLabV3Plus model("mobilenet_v3_large", num_classes);
    model->eval();

    cv::Mat img = cv::imread("street.jpg");
    if (img.empty()) return;

    cv::Mat img_resized;
    cv::resize(img, img_resized, cv::Size(512, 512));

    torch::Tensor input = torch::from_blob(img_resized.data, {1, 512, 512, 3}, torch::kByte);
    input = input.permute({0, 3, 1, 2}).to(torch::kFloat32).div(255.0);

    torch::NoGradGuard no_grad;
    auto outputs = model->forward(input);

    if (outputs.find("out") != outputs.end()) {
        torch::Tensor out = outputs["out"];

        cv::Mat color_mask = SegmentationUtils::tensor_to_mask(out);
        cv::imwrite("seg_result_color.png", color_mask);

        cv::Mat binary_mask = SegmentationUtils::extract_binary_mask(out, 15);
        cv::imwrite("seg_result_person_binary.png", binary_mask);

        cv::Mat overlay;
        cv::addWeighted(img_resized, 0.5, color_mask, 0.5, 0.0, overlay);
        cv::imwrite("seg_result_overlay.png", overlay);

        std::cout << "Segmentation completed. Saved results." << std::endl;
    }
}