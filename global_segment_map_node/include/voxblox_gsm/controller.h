// Copyright 2018 Margarita Grinvald, ASL, ETH Zurich, Switzerland

#ifndef VOXBLOX_GSM_INCLUDE_VOXBLOX_GSM_CONTROLLER_H_
#define VOXBLOX_GSM_INCLUDE_VOXBLOX_GSM_CONTROLLER_H_

#include <vector>

#include <geometry_msgs/Transform.h>
#include <global_feature_map/feature_block.h>
#include <global_feature_map/feature_integrator.h>
#include <global_feature_map/feature_layer.h>
#include <global_feature_map/feature_types.h>
#include <global_feature_map/feature_utils.h>
#include <global_segment_map/label_tsdf_integrator.h>
#include <global_segment_map/label_tsdf_map.h>
#include <global_segment_map/label_voxel.h>
#include <global_segment_map/meshing/label_tsdf_mesh_integrator.h>
#include <global_segment_map/utils/visualizer.h>
#include <modelify_msgs/Features.h>
#include <modelify_msgs/GsmUpdate.h>
#include <modelify_msgs/ValidateMergedObject.h>
#include <ros/ros.h>
#include <sensor_msgs/PointCloud2.h>
#include <std_srvs/Empty.h>
#include <std_srvs/SetBool.h>
#include <tf/transform_listener.h>
#include <tf2_ros/transform_broadcaster.h>
#include <voxblox/io/mesh_ply.h>
#include <voxblox_ros/conversions.h>

namespace voxblox {
namespace voxblox_gsm {

typedef std::tuple<Layer<TsdfVoxel>, Layer<LabelVoxel>, FeatureLayer<Feature3D>>
    LayerTuple;

enum LayerAccessor {
  kTsdfLayer = 0,
  kLabelLayer = 1,
  kFeatureLayer = 2,
  kCount
};

class Controller {
 public:
  Controller(ros::NodeHandle* node_handle);

  ~Controller();

  void subscribeFeatureTopic(ros::Subscriber* feature_sub);

  void advertiseFeatureBlockTopic(ros::Publisher* feature_block_pub);

  void subscribeSegmentPointCloudTopic(
      ros::Subscriber* segment_point_cloud_sub);

  void advertiseSegmentGsmUpdateTopic(ros::Publisher* segment_gsm_update_pub);

  void advertiseSceneGsmUpdateTopic(ros::Publisher* scene_gsm_update_pub);

  void advertiseSegmentMeshTopic(ros::Publisher* segment_mesh_pub);

  void advertiseSceneMeshTopic(ros::Publisher* scene_mesh_pub);

  void advertiseBboxTopic(ros::Publisher* bbox_pub);

  void advertisePublishSceneService(ros::ServiceServer* publish_scene_srv);

  void validateMergedObjectService(
      ros::ServiceServer* validate_merged_object_srv);

  void advertiseGenerateMeshService(ros::ServiceServer* generate_mesh_srv);

  void advertiseSaveSegmentsAsMeshService(
      ros::ServiceServer* save_segments_as_mesh_srv);

  void advertiseExtractInstancesService(
      ros::ServiceServer* extract_instances_srv);

  bool publishObjects(const bool publish_all = false);

  void publishScene();

  bool noNewUpdatesReceived() const;

  virtual void extractSegmentLayers(
      const std::vector<Label>& labels,
      std::unordered_map<Label, LayerTuple>* label_layers_map,
      bool labels_list_is_complete = false);

  bool enable_semantic_instance_segmentation_;

  bool publish_gsm_updates_;

  bool publish_scene_mesh_;
  bool publish_segment_mesh_;
  bool compute_and_publish_bbox_;
  bool publish_feature_blocks_marker_;

  bool use_label_propagation_;

  double no_update_timeout_;

 protected:
  virtual void featureCallback(const modelify_msgs::Features& features_msg);

  virtual void segmentPointCloudCallback(
      const sensor_msgs::PointCloud2::Ptr& segment_point_cloud_msg);

  virtual bool publishSceneCallback(std_srvs::SetBool::Request& request,
                                    std_srvs::SetBool::Response& response);

  bool validateMergedObjectCallback(
      modelify_msgs::ValidateMergedObject::Request& request,
      modelify_msgs::ValidateMergedObject::Response& response);

  bool generateMeshCallback(std_srvs::Empty::Request& request,
                            std_srvs::Empty::Response& response);

  bool saveSegmentsAsMeshCallback(std_srvs::Empty::Request& request,
                                  std_srvs::Empty::Response& response);

  bool extractInstancesCallback(std_srvs::Empty::Request& request,
                                std_srvs::Empty::Response& response);

  bool lookupTransform(const std::string& from_frame,
                       const std::string& to_frame, const ros::Time& timestamp,
                       Transformation* transform);

  void generateMesh(bool clear_mesh);

  void updateMeshEvent(const ros::TimerEvent& e);

  virtual void publishGsmUpdate(const ros::Publisher& publisher,
                                modelify_msgs::GsmUpdate* gsm_update);

  virtual void getLabelsToPublish(const bool get_all,
                                  std::vector<Label>* labels);

  void computeAlignedBoundingBox(
      const pcl::PointCloud<pcl::PointSurfel>::Ptr surfel_cloud,
      Eigen::Vector3f* bbox_translation, Eigen::Quaternionf* bbox_quaternion,
      Eigen::Vector3f* bbox_size);

  ros::NodeHandle* node_handle_private_;

  tf::TransformListener tf_listener_;
  tf2_ros::TransformBroadcaster tf_broadcaster_;
  ros::Time last_segment_msg_timestamp_;
  size_t integrated_frames_count_;

  std::string world_frame_;

  // TODO(margaritaG): make this optional.
  // Shutdown logic: if no messages are received for X amount of time,
  // shut down node.
  bool received_first_message_;
  ros::Time last_update_received_;

  LabelTsdfMap::Config map_config_;
  LabelTsdfIntegrator::Config tsdf_integrator_config_;
  LabelTsdfIntegrator::LabelTsdfConfig label_tsdf_integrator_config_;

  std::shared_ptr<LabelTsdfMap> map_;
  std::shared_ptr<LabelTsdfIntegrator> integrator_;

  bool received_first_feature_;
  std::shared_ptr<FeatureLayer<Feature3D>> feature_layer_;
  std::shared_ptr<FeatureIntegrator> feature_integrator_;

  MeshIntegratorConfig mesh_config_;
  MeshLabelIntegrator::LabelTsdfConfig label_tsdf_mesh_config_;
  ros::Timer update_mesh_timer_;
  ros::Publisher* scene_mesh_pub_;
  ros::Publisher* segment_mesh_pub_;
  MeshLabelIntegrator::ColorScheme mesh_color_scheme_;
  std::string mesh_filename_;

  std::shared_ptr<MeshLayer> mesh_label_layer_;
  std::shared_ptr<MeshLayer> mesh_semantic_layer_;
  std::shared_ptr<MeshLayer> mesh_instance_layer_;
  std::shared_ptr<MeshLayer> mesh_merged_layer_;
  std::shared_ptr<MeshLabelIntegrator> mesh_label_integrator_;
  std::shared_ptr<MeshLabelIntegrator> mesh_semantic_integrator_;
  std::shared_ptr<MeshLabelIntegrator> mesh_instance_integrator_;
  std::shared_ptr<MeshLabelIntegrator> mesh_merged_integrator_;

  // Semantic labels.
  std::set<SemanticLabel> all_semantic_labels_;
  std::map<Label, std::map<SemanticLabel, int>>* label_class_count_ptr_;

  // Current frame label propagation.
  std::vector<Segment*> segments_to_integrate_;
  std::map<Label, std::map<Segment*, size_t>> segment_label_candidates;
  std::map<Segment*, std::vector<Label>> segment_merge_candidates_;

  // Object Database..
  ros::Publisher* scene_gsm_update_pub_;
  ros::Publisher* segment_gsm_update_pub_;
  std::vector<Label> segment_labels_to_publish_;
  std::map<Label, std::set<Label>> merges_to_publish_;
  std::set<Label> all_published_segments_;
  ros::Publisher* bbox_pub_;
  ros::Publisher* feature_block_pub_;

  std::thread viz_thread_;
  Visualizer* visualizer_;
  std::mutex updated_mesh_mutex_;
  bool updated_mesh_;
  bool need_full_remesh_;

  int min_number_of_allocated_blocks_to_publish_;
};

}  // namespace voxblox_gsm
}  // namespace voxblox

#endif  // VOXBLOX_GSM_INCLUDE_VOXBLOX_GSM_CONTROLLER_H_
