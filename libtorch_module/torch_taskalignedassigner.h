#ifndef TORCH_TASKALIGNEDASSIGNER_H
#define TORCH_TASKALIGNEDASSIGNER_H


#include <torch/torch.h>
#include <vector>
#include <cmath>
#include <stdexcept>
#include <tuple>


class TaskAlignedAssignerImpl : public torch::nn::Module {
public:
    TaskAlignedAssignerImpl(int64_t topk = 10, int64_t num_classes = 80, float alpha = 1.0f, float beta = 6.0f)
        : topk_(topk), num_classes_(num_classes), alpha_(alpha), beta_(beta) {

        TORCH_CHECK(topk > 0, "TaskAlignedAssigner topk must be positive");
        TORCH_CHECK(num_classes > 0, "TaskAlignedAssigner num_classes must be positive");

    }

    torch::Tensor forward(
        const torch::Tensor& pd_scores_in,
        const torch::Tensor& pd_bboxes_in,
        const torch::Tensor& gt_labels_in,
        const torch::Tensor& gt_bboxes_in,
        const torch::Tensor& mask_gt_in) {

        torch::Tensor pd_scores = pd_scores_in;
        if (pd_scores.dim() == 4 && pd_scores.size(1) == 1) {
            pd_scores = pd_scores.squeeze(1);
        }

        torch::Tensor pd_bboxes = pd_bboxes_in;
        if (pd_bboxes.dim() == 4 && pd_bboxes.size(1) == 1) {
            pd_bboxes = pd_bboxes.squeeze(1);
        }

        int64_t batch_size = pd_scores.size(0);
        int64_t num_anchors = pd_scores.size(1);
        int64_t num_classes = pd_scores.size(2);
        int64_t max_gt = gt_labels_in.size(1);

        torch::Device device = pd_scores.device();

        torch::Tensor gt_labels = gt_labels_in.to(device).reshape({ batch_size, max_gt });
        torch::Tensor gt_bboxes = gt_bboxes_in.to(device).reshape({ batch_size, max_gt, 4 });
        torch::Tensor mask_gt = mask_gt_in.to(device).reshape({ batch_size, max_gt });

        torch::Tensor assigned_gt_inds = torch::zeros({ batch_size, num_anchors },
            torch::dtype(torch::kLong).device(device));

        for (int64_t b = 0; b < batch_size; ++b) {
            torch::Tensor valid_mask = mask_gt[b].to(torch::kBool);

            if (!valid_mask.any().item<bool>()) continue;

            torch::Tensor gt_labels_b = gt_labels[b].masked_select(valid_mask).to(torch::kLong);
            torch::Tensor valid_indices = torch::nonzero(valid_mask).squeeze(1);
            torch::Tensor gt_bboxes_b = gt_bboxes[b].index_select(0, valid_indices);

            int64_t num_valid_gt = gt_labels_b.size(0);


            torch::Tensor iou = bbox_iou(pd_bboxes[b], gt_bboxes_b);


            torch::Tensor gt_labels_expand = gt_labels_b.unsqueeze(0).expand({ num_anchors, num_valid_gt });

            torch::Tensor cls_scores = pd_scores[b].gather(1, gt_labels_expand);

            // A task-aligned candidate is only valid when its decoded anchor
            // centre lies inside the GT box.  Without this constraint, top-k
            // selected distant anchors whose tiny non-zero metric later became
            // high-confidence duplicate detections.
            const torch::Tensor anchor_centers_x =
                (pd_bboxes[b].select(1, 0) + pd_bboxes[b].select(1, 2)) * 0.5f;
            const torch::Tensor anchor_centers_y =
                (pd_bboxes[b].select(1, 1) + pd_bboxes[b].select(1, 3)) * 0.5f;
            const torch::Tensor inside_gt =
                (anchor_centers_x.unsqueeze(1) >= gt_bboxes_b.select(1, 0).unsqueeze(0)) &
                (anchor_centers_x.unsqueeze(1) <= gt_bboxes_b.select(1, 2).unsqueeze(0)) &
                (anchor_centers_y.unsqueeze(1) >= gt_bboxes_b.select(1, 1).unsqueeze(0)) &
                (anchor_centers_y.unsqueeze(1) <= gt_bboxes_b.select(1, 3).unsqueeze(0));
            const torch::Tensor valid_candidate = inside_gt & (iou > 0.0f);
            torch::Tensor metric = torch::pow(cls_scores, alpha_) * torch::pow(iou, beta_);
            metric = torch::where(valid_candidate, metric, torch::full_like(metric, -1.0f));

            int64_t k = std::min(topk_, num_anchors);
            auto topk_res = torch::topk(metric, k, 0, true);
            torch::Tensor topk_metrics = std::get<0>(topk_res);
            torch::Tensor topk_idxs = std::get<1>(topk_res);

            torch::Tensor mask_topk = torch::zeros_like(metric, torch::kBool);
            mask_topk.scatter_(0, topk_idxs, topk_metrics > 1e-9f);


            torch::Tensor sum_mask = mask_topk.sum(1);
            if ((sum_mask > 1).any().item<bool>()) {
                auto max_res = metric.max(1, true);
                torch::Tensor max_metric = std::get<0>(max_res);
                mask_topk &= (metric == max_metric);
            }


            auto match_indices = torch::nonzero(mask_topk);
            if (match_indices.size(0) > 0) {
                torch::Tensor anchor_idxs = match_indices.select(1, 0);
                torch::Tensor gt_idxs_in_valid = match_indices.select(1, 1);

                torch::Tensor real_gt_idxs = valid_indices.index_select(0, gt_idxs_in_valid) + 1;

                assigned_gt_inds[b].index_put_({ anchor_idxs }, real_gt_idxs);
            }
        }

        return assigned_gt_inds;
    }

private:
    int64_t topk_;
    int64_t num_classes_;
    float alpha_;
    float beta_;

    torch::Tensor bbox_iou(const torch::Tensor& box1, const torch::Tensor& box2) {

        int64_t A = box1.size(0);
        int64_t G = box2.size(0);

        auto b1 = box1.unsqueeze(1);
        auto b2 = box2.unsqueeze(0);

        auto inter_x1 = torch::max(b1.slice(2, 0, 1), b2.slice(2, 0, 1));
        auto inter_y1 = torch::max(b1.slice(2, 1, 2), b2.slice(2, 1, 2));
        auto inter_x2 = torch::min(b1.slice(2, 2, 3), b2.slice(2, 2, 3));
        auto inter_y2 = torch::min(b1.slice(2, 3, 4), b2.slice(2, 3, 4));

        auto inter_w = (inter_x2 - inter_x1).clamp_min(0);
        auto inter_h = (inter_y2 - inter_y1).clamp_min(0);
        auto inter_area = inter_w * inter_h;

        auto w1 = b1.slice(2, 2, 3) - b1.slice(2, 0, 1);
        auto h1 = b1.slice(2, 3, 4) - b1.slice(2, 1, 2);
        auto area1 = w1 * h1;

        auto w2 = b2.slice(2, 2, 3) - b2.slice(2, 0, 1);
        auto h2 = b2.slice(2, 3, 4) - b2.slice(2, 1, 2);
        auto area2 = w2 * h2;

        auto union_area = area1 + area2 - inter_area + 1e-7;

        return (inter_area / union_area).squeeze(2);
    }
};

TORCH_MODULE(TaskAlignedAssigner);

#endif
