#include "voxblox_gsm/sliding_window_controller.h"

#include <minkindr_conversions/kindr_tf.h>
#include <std_msgs/builtin_uint32.h>

namespace voxblox {
namespace voxblox_gsm {

SlidingWindowController::SlidingWindowController(ros::NodeHandle* node_handle)
    : Controller(node_handle) {
  double update_period = 1.0;
  node_handle_private_->param("sliding_window/tf_check_t", update_period,
                              update_period);
  ros::Duration period(update_period);
  tf_check_timer_ = node_handle_private_->createTimer(
      period, &SlidingWindowController::checkTfCallback, this);
  node_handle_private_->param<float>("sliding_window/radius", window_radius_,
                                     window_radius_);
}

void SlidingWindowController::removeSegmentsOutsideOfRadius(float radius,
                                                            Point center) {
  std::vector<Label> all_labels = integrator_->getLabelsList();
  label_to_layers_.clear();
  extractAllSegmentLayers(all_labels, &label_to_layers_);

  for (const Label& label : all_labels) {
    auto it = label_to_layers_.find(label);
    LayerPair& layer_pair = it->second;
    if (!hasMinNumberOfAllocatedBlocksToPublish(layer_pair.first)) {
      continue;
    }

    // Iterate over all blocks of segment. If one of the blocks is inside the
    // window radius, the whole segment is valid. Otherwise, the segment is
    // removed from the gsm
    BlockIndexList blocks_of_label;
    layer_pair.first.getAllAllocatedBlocks(&blocks_of_label);
    bool has_block_within_radius = false;
    for (const BlockIndex& block_index : blocks_of_label) {
      Point center_block = getCenterPointFromGridIndex(
          block_index, map_config_.voxel_size * map_config_.voxels_per_side);
      double distance_x_y = sqrt(pow(center_block(0, 0) - center(0, 0), 2) +
                                 pow(center_block(1, 0) - center(1, 0), 2));
      if (distance_x_y < radius) {
        has_block_within_radius = true;
        break;
      }
    }
    if (!has_block_within_radius) {
      for (const BlockIndex& block_index : blocks_of_label) {
        map_->getTsdfLayerPtr()->removeBlock(block_index);
        map_->getLabelLayerPtr()->removeBlock(block_index);
      }
      removed_segments_pub_.publish(label);
    }
  }
}

void SlidingWindowController::extractAllSegmentLayers(
    const std::vector<Label>& labels,
    std::unordered_map<Label, LayerPair>* label_layers_map,
    bool labels_list_is_complete) {
  if (!label_to_layers_.empty()) {
    *label_layers_map = label_to_layers_;
  } else {
    Controller::extractAllSegmentLayers(labels, label_layers_map,
                                        labels_list_is_complete);
  }
}

void SlidingWindowController::checkTfCallback(const ros::TimerEvent&) {
  tf::StampedTransform tf_transform;
  getCurrentPosition(&tf_transform);

  tfScalar distance =
      tf_transform.getOrigin().distance(current_window_position_.getOrigin());
  LOG(WARNING) << "distance " << distance;

  if (distance > window_radius_) {
    updateAndPublishWindow(current_window_position_point_);
    publishPosition(current_window_position_point_);
    current_window_position_ = tf_transform;
    Transformation kindr_transform;
    tf::transformTFToKindr(tf_transform, &kindr_transform);
    current_window_position_point_ = kindr_transform.getPosition();
  }
}

void SlidingWindowController::updateAndPublishWindow(const Point& new_center) {
  removeSegmentsOutsideOfRadius(window_radius_, new_center);
  std_srvs::Empty::Request req;
  std_srvs::Empty::Response res;
  LOG(INFO) << "Publish scene";
  publishSceneCallback(req, res);
}

void SlidingWindowController::getCurrentPosition(
    tf::StampedTransform* position) {
  // Get current position. The window radius is relative to this position.
  ros::Time time_now = ros::Time(0);
  try {
    tf_listener_.waitForTransform(
        world_frame_, camera_frame_, time_now,
        ros::Duration(30.0));  // in case rosbag has not been started yet
    tf_listener_.lookupTransform(world_frame_, camera_frame_, time_now,
                                 *position);
  } catch (tf::TransformException& ex) {
    LOG(FATAL) << "Error getting TF transform from sensor data: " << ex.what();
  }
}

void SlidingWindowController::publishGsmUpdate(
    const ros::Publisher& publisher, modelify_msgs::GsmUpdate& gsm_update) {
  geometry_msgs::Point point;
  point.x = current_window_position_point_(0);
  point.y = current_window_position_point_(1);
  point.z = current_window_position_point_(2);
  gsm_update.sliding_window_position = point;
  Controller::publishGsmUpdate(publisher, gsm_update);
}

void SlidingWindowController::publishPosition(const Point& position) {
  // Publish position.
  tf::StampedTransform tf;
  tf::Vector3 translation;
  translation.setX(position(0));
  translation.setY(position(1));
  translation.setZ(position(2));
  tf.setOrigin(translation);
  position_broadcaster_.sendTransform(
      tf::StampedTransform(tf, ros::Time::now(), "world", "sliding_window"));
}

}  // namespace voxblox_gsm
}  // namespace voxblox