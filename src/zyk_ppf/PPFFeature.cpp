#include "PPFFeature.h"
#include <pcl/features/ppf.h>
#include <pcl/common/transforms.h>
#include <fstream>
#ifdef vote_flag_use_map
#include <unordered_map>
#endif
#include <pcl/registration/icp.h> 
using namespace zyk;


zyk::PPF_Space::PPF_Space()
	:ignore_plane_switch(false)
	, mName("")
    , ver_(1)
	, grid_f1_div(10)
	, grid_f2_div(10)
	, grid_f3_div(10)
	, grid_f4_div(10)
	, total_box_num(10000)
	, cx(0)
	, cy(0)
	, cz(0)
	, centered_point_cloud(new pcl::PointCloud<PointType>())
{
#ifdef plane_check
    plane_vote_thresh=10;
#endif
}

zyk::PPF_Space::~PPF_Space()
{
	for (int i = 0; i < ppf_box_vector.size();i++)
    if (ppf_box_vector[i] != NULL) delete ppf_box_vector[i];
}
#ifdef view_based
bool PPF_Space::init(string Name, pcl::PointCloud<PointType>::Ptr pointcloud, pcl::PointCloud<NormalType>::Ptr pointNormal, std::vector<std::vector<int> > &view_based_indexes, int angle_div, int distance_div, bool ignore_plane)
{
  mName = Name;
  ver_ = 2;
  input_point_cloud = pointcloud;
  input_point_normal = pointNormal;
  //div
  assert(angle_div > 0 && distance_div > 0);
  //switch
  ignore_plane_switch = ignore_plane;
  //ppf
  if (!computeALL_Visible_PPF(view_based_indexes))
    return false;

  if (!constructGrid(angle_div,distance_div))
    return false;
  ///// INFO
  //cout << "During training, total ppf box number is: " << total_box_num << endl;
  //zyk::PPF ppf;
  //ppf.ppf.f1 = max_p(0);
  //ppf.ppf.f2 = max_p(1);
  //ppf.ppf.f3 = max_p(2);
  //ppf.ppf.f4 = max_p(3);
  //cout << "The max_p's index is: " << getppfBoxIndex(ppf) << endl;
  return true;

}
#endif
//bool zyk::PPF_Space::init(pcl::PointCloud<PointType>::Ptr pointcloud, pcl::PointCloud<NormalType>::Ptr pointNormal, float angle_step, float distance_step, bool ignore_plane)
//{
//	assert(angle_step>0 && distance_step>0);
//	leaf_size(0) = angle_step;
//	leaf_size(1) = angle_step;
//	leaf_size(2) = angle_step;
//	leaf_size(3) = distance_step;
//
//	//ptr
//	centered_point_cloud = pointcloud;
//	input_point_normal = pointNormal;
//	//switch
//	ignore_plane_switch = ignore_plane;
//	//ppf
//	if (!computeAllPPF())
//		return false;
//	//size
//	if (!findBoundingBox())
//		return false;
//	Eigen::Array4f dim = max_p - min_p;
//	grid_f1_div = floor(dim(0) / leaf_size(0) + 0.5);
//	grid_f2_div = floor(dim(1) / leaf_size(1) + 0.5);
//	grid_f3_div = floor(dim(2) / leaf_size(2) + 0.5);
//	grid_f4_div = floor(dim(3) / leaf_size(3) + 0.5);
//
//	grid_div_mul(0) = 1;
//	grid_div_mul(1) = grid_div_mul(0)*grid_f1_div;
//	grid_div_mul(2) = grid_div_mul(1)*grid_f2_div;
//	grid_div_mul(3) = grid_div_mul(2)*grid_f3_div;
//	total_box_num = grid_div_mul(3)*grid_f4_div;
//	assert(total_box_num > 0);
//
//	inv_leaf_size(0) = 1 / leaf_size(0);
//	inv_leaf_size(1) = 1 / leaf_size(1);
//	inv_leaf_size(2) = 1 / leaf_size(2);
//	inv_leaf_size(3) = 1 / leaf_size(3);
//
//	//grid
//	ppf_box_vector.resize(total_box_num);
//	if (!constructGrid())
//		return false;
//	return true;
//
//}
bool zyk::PPF_Space::init(std::string Name, pcl::PointCloud<PointType>::Ptr pointcloud, pcl::PointCloud<NormalType>::Ptr pointNormal, int angle_div, int distance_div, bool ignore_plane)
{
	mName = Name;
	ver_ = 1;
	input_point_cloud = pointcloud;
	input_point_normal = pointNormal;
	//div
	assert(angle_div > 0 && distance_div > 0);
	//switch
	ignore_plane_switch = ignore_plane;
	//ppf
	if (!computeAllPPF())
		return false;

	if (!constructGrid(angle_div,distance_div))
		return false;
	///// INFO
	//cout << "During training, total ppf box number is: " << total_box_num << endl;
	//zyk::PPF ppf;
	//ppf.ppf.f1 = max_p(0);
	//ppf.ppf.f2 = max_p(1);
	//ppf.ppf.f3 = max_p(2);
	//ppf.ppf.f4 = max_p(3);
	//cout << "The max_p's index is: " << getppfBoxIndex(ppf) << endl;
	return true;
}

void zyk::PPF_Space::clear()
{
	centered_point_cloud = NULL;
	input_point_normal = NULL;
	for (int i = 0; i < ppf_box_vector.size(); i++){
		if (ppf_box_vector[i] != NULL) delete ppf_box_vector[i];
	}
	ppf_box_vector.clear();
	ppf_vector.clear();
}

bool zyk::PPF_Space::findBoundingBox()
{
	if (ppf_vector.empty())
		return false;
	min_p.setConstant(FLT_MAX);
	max_p.setConstant(-FLT_MAX);
	for (int i = 0; i < ppf_vector.size(); i++)
	{
		pcl::PPFSignature p = ppf_vector.at(i).ppf;
		if (!pcl_isfinite(p.f1) || !pcl_isfinite(p.f2) || !pcl_isfinite(p.f3) || !pcl_isfinite(p.f4) || !pcl_isfinite(p.alpha_m))
		{
			continue;
		}
		min_p = min_p.min(Eigen::Array4f(p.f1, p.f2, p.f3, p.f4));
		max_p = max_p.max(Eigen::Array4f(p.f1, p.f2, p.f3, p.f4));
	}
	//cout << "ppf min_p: " << min_p << endl;
	//cout << "ppf max_p: " << max_p << endl;
	return true;
}

bool zyk::PPF_Space::constructGrid(int angle_div,int distance_div)
{
//  float diameter = zyk::norm(model_size,3);
  double min_coord[3],max_coord[3];
  zyk::getBoundingBox(input_point_cloud,min_coord,max_coord);
  double diameter = zyk::dist(min_coord,max_coord,3);
  cx = (min_coord[0] + max_coord[0]) / 2;
  cy = (min_coord[1] + max_coord[1]) / 2;
  cz = (min_coord[2] + max_coord[2]) / 2;
  centered_point_cloud->clear();
  for (size_t i = 0; i < input_point_cloud->size(); ++i) {
    PointType _tem;
    _tem.x = input_point_cloud->at(i).x - cx;
    _tem.y = input_point_cloud->at(i).y - cy;
    _tem.z = input_point_cloud->at(i).z - cz;
    centered_point_cloud->push_back(_tem);
  }
  std::vector<float>tmp;
  tmp.push_back(max_coord[0]-min_coord[0]);
  tmp.push_back(max_coord[1]-min_coord[1]);
  tmp.push_back(max_coord[2]-min_coord[2]);
  std::sort(tmp.begin(),tmp.end());
  model_size[0]=tmp[0];
  model_size[1]=tmp[1];
  model_size[2]=tmp[2];
  //size
  if (!findBoundingBox())
    return false;
  leaf_size(0)=M_PI/angle_div;
  leaf_size(1) = leaf_size(0);
  leaf_size(2) = leaf_size(0);
  leaf_size(3) = diameter/distance_div;
  inv_leaf_size(0) = 1 / leaf_size(0);
  inv_leaf_size(1) = 1 / leaf_size(1);
  inv_leaf_size(2) = 1 / leaf_size(2);
  inv_leaf_size(3) = 1 / leaf_size(3);

  grid_f1_div = angle_div;
  grid_f2_div = angle_div;
  grid_f3_div = angle_div;
  grid_f4_div = distance_div;
  grid_div_mul(0) = 1;
  grid_div_mul(1) = grid_div_mul(0)*grid_f1_div;
  grid_div_mul(2) = grid_div_mul(1)*grid_f2_div;
  grid_div_mul(3) = grid_div_mul(2)*grid_f3_div;
  total_box_num = grid_div_mul(3)*grid_f4_div;
  assert(total_box_num > 0);

  //grid
  ppf_box_vector.resize(total_box_num);

	if (ppf_vector.empty())
		return false;
	for (int i = 0; i < ppf_vector.size(); i++)
	{
		int idx = getppfBoxIndex(ppf_vector[i]);
		//MODIFIED 17-10-22
		//During construction, the idx cannot be -1
		if (idx != -1)
		{
			if (ppf_box_vector[idx] == NULL)
				ppf_box_vector[idx] = new box;
			ppf_box_vector[idx]->putin(i);
		}
		else
		{
			PCL_WARN("Index becomes -1 during training, check if there is infinite points!\r\n");
		}
	}
	return true;
}
float zyk::PPF_Space::computeAlpha(const PointType& first_pnt, const NormalType& first_normal, const PointType& second_pnt)
{
	float rotation_angle = acosf(first_normal.normal_x);
	bool parallel_to_x = (first_normal.normal_y > -FLT_EPSILON && first_normal.normal_y<FLT_EPSILON && first_normal.normal_z>-FLT_EPSILON && first_normal.normal_z < FLT_EPSILON);
	//@todo, change to total array
	//finished 2017 12 31 zyk
#ifdef use_eigen
	const Eigen::Vector3f& rotation_axis = (parallel_to_x) ? (Eigen::Vector3f::UnitY()) : (first_normal.getNormalVector3fMap().cross(Eigen::Vector3f::UnitX()).normalized());
	Eigen::AngleAxisf rotation_mg(rotation_angle, rotation_axis);
	const Eigen::Vector3f& second_point_transformed = rotation_mg * (Eigen::Vector3f(second_pnt.x - first_pnt.x, second_pnt.y - first_pnt.y, second_pnt.z - first_pnt.z));
	float angle = atan2f(-second_point_transformed(2), second_point_transformed(1));

	if (sin(angle) * second_point_transformed(2) > 0.0f)
		angle *= (-1);
#else
	float nx = 0, ny = 1, nz = 0;
	if (!parallel_to_x) {
		ny = first_normal.normal_z;
		nz = -first_normal.normal_y;
		float tmp = sqrt(ny*ny + nz*nz);
		ny /= tmp;
		nz /= tmp;
	}
	float X = second_pnt.x - first_pnt.x,
		Y = second_pnt.y - first_pnt.y,
		Z = second_pnt.z - first_pnt.z,
		stheta = sinf(rotation_angle),
		ctheta = cosf(rotation_angle),
		one_ctheta = (1 - ctheta);
	float Y_out = (one_ctheta*nx*ny + stheta*nz)*X
		+ (ctheta + one_ctheta*ny*ny)*Y
		+ (one_ctheta*ny*nz - stheta*nx)*Z;
	float Z_out = (one_ctheta*nx*nz - stheta*ny)*X
		+ (one_ctheta*ny*nz + stheta*nx)*Y
		+ (ctheta + one_ctheta*nz*nz)*Z;
	float angle = atan2f(-Z_out, Y_out);
	if (sin(angle) * Z_out > 0.0f)
		angle *= (-1);
#endif
	return angle;

}

float zyk::PPF_Space::computeAlpha(const Eigen::Vector3f& first_pnt, const Eigen::Vector3f& first_normal, const Eigen::Vector3f& second_pnt)
{
	float rotation_angle = acosf(first_normal(0));// float rotation_angle = acosf(model_reference_normal.dot(Eigen::Vector3f::UnitX()));
	bool parallel_to_x = (first_normal(1) > -FLT_EPSILON && first_normal(1)<FLT_EPSILON && first_normal(2)>-FLT_EPSILON && first_normal(2) < FLT_EPSILON);
	const Eigen::Vector3f& rotation_axis = (parallel_to_x) ? (Eigen::Vector3f::UnitY()) : (first_normal.cross(Eigen::Vector3f::UnitX()).normalized());
	Eigen::AngleAxisf rotation_mg(rotation_angle, rotation_axis);
	//Eigen::Affine3f transform_mg = Eigen::Affine3f::Identity();
	//transform_mg.rotate(rotation_mg);
	//transform_mg.translate(-rotation_mg.matrix()*model_reference_point);
	//Eigen::Affine3f transform_mg(Eigen::Translation3f(rotation_mg * ((-1) * model_reference_point)) * rotation_mg);

	const Eigen::Vector3f& second_point_transformed = rotation_mg * (second_pnt - first_pnt);
	float angle = atan2f(-second_point_transformed(2), second_point_transformed(1));
	if (sin(angle) * second_point_transformed(2) > 0.0f)
		angle *= (-1);
	return angle;
}
void zyk::PPF_Space::computeSinglePPF(const PointType& first_pnt, const NormalType& first_normal, const PointType& second_pnt, const NormalType& second_normal, zyk::PPF& ppf)
{
	float d[3];
	d[0] = second_pnt.x - first_pnt.x;
	d[1] = second_pnt.y - first_pnt.y;
	d[2] = second_pnt.z - first_pnt.z;
	float d_norm = zyk::norm(d, 3);

	d[0] /= d_norm;
	d[1] /= d_norm;
	d[2] /= d_norm;

	float dot_res;
	dot_res = zyk::dot(first_normal.normal, d, 3);
	if (dot_res > 1)dot_res = 1;
	else if (dot_res < -1)dot_res = -1;
	ppf.ppf.f1 = acosf(dot_res);

	dot_res = zyk::dot(second_normal.normal, d, 3);
	if (dot_res>1)dot_res = 1;
	else if (dot_res < -1)dot_res = -1;
	ppf.ppf.f2 = acosf(dot_res);

	dot_res = dot(first_normal, second_normal);
	if (dot_res>1)dot_res = 1;
	else if (dot_res < -1)dot_res = -1;
	ppf.ppf.f3 = acosf(dot_res);

	ppf.ppf.f4 = d_norm;

}
void zyk::PPF_Space::computeSinglePPF(const Eigen::Vector3f& first_pnt, const Eigen::Vector3f& first_normal, const Eigen::Vector3f& second_pnt, const Eigen::Vector3f& second_normal,zyk::PPF& ppf)
{
	Eigen::Vector3f d_normed = second_pnt - first_pnt;
	float d_norm = d_normed.norm();
	d_normed /= d_norm;
	float dot_res;
	dot_res = first_normal.dot(d_normed);
	if (dot_res>1)dot_res = 1;
	else if (dot_res<-1)dot_res = -1;
	ppf.ppf.f1 = acosf(dot_res);
	// may be a little bit larger than PI 2017-10-22
	if (ppf.ppf.f1 >= M_PI)ppf.ppf.f1 = 3.1415;

	dot_res = second_normal.dot(d_normed);
	if (dot_res>1)dot_res = 1;
	else if (dot_res<-1)dot_res = -1;
	ppf.ppf.f2 = acosf(dot_res);
	if (ppf.ppf.f2 >= M_PI)ppf.ppf.f2 = 3.1415;

	dot_res = first_normal.dot(second_normal);
	if (dot_res>1)dot_res = 1;
	else if (dot_res<-1)dot_res = -1;
	ppf.ppf.f3 = acosf(dot_res);
	if (ppf.ppf.f3 >= M_PI)ppf.ppf.f3 = 3.1415;

	ppf.ppf.f4 = d_norm;

	/////// for speed reason, the following is separated!
	//float rotation_angle = acosf(first_normal(0));// float rotation_angle = acosf(model_reference_normal.dot(Eigen::Vector3f::UnitX()));
	//bool parallel_to_x = (first_normal(1) > -FLT_EPSILON && first_normal(1)<FLT_EPSILON && first_normal(2)>-FLT_EPSILON && first_normal(2) < FLT_EPSILON);
	//const Eigen::Vector3f& rotation_axis = (parallel_to_x) ? (Eigen::Vector3f::UnitY()) : (first_normal.cross(Eigen::Vector3f::UnitX()).normalized());
	//Eigen::AngleAxisf rotation_mg(rotation_angle, rotation_axis);
	////Eigen::Affine3f transform_mg = Eigen::Affine3f::Identity();
	////transform_mg.rotate(rotation_mg);
	////transform_mg.translate(-rotation_mg.matrix()*model_reference_point);
	////Eigen::Affine3f transform_mg(Eigen::Translation3f(rotation_mg * ((-1) * model_reference_point)) * rotation_mg);

	//const Eigen::Vector3f& second_point_transformed = rotation_mg * (second_pnt - first_pnt);
	//float angle = atan2f(-second_point_transformed(2), second_point_transformed(1));
	//if (sin(angle) * second_point_transformed(2) > 0.0f)
	//	angle *= (-1);
	//ppf.ppf.alpha_m = angle;
}

void zyk::PPF_Space::computeSinglePPF(pcl::PointCloud<PointType>::Ptr pointcloud, pcl::PointCloud<NormalType>::Ptr pointNormal, int index1, int index2, PPF& ppf)
{
	const Eigen::Vector3f& first_pnt = pointcloud->at(index1).getVector3fMap();
	const Eigen::Vector3f& first_normal = pointNormal->at(index1).getNormalVector3fMap();
	const Eigen::Vector3f& second_pnt = pointcloud->at(index2).getVector3fMap();
	const Eigen::Vector3f& second_normal = pointNormal->at(index2).getNormalVector3fMap();

	computeSinglePPF(first_pnt, first_normal, second_pnt, second_normal, ppf);
	ppf.first_index = index1;
	ppf.second_index = index2;

	//compute weight
	ppf.weight= 1 - 0.98 * fabs(cos(ppf.ppf.f3));
}

bool zyk::PPF_Space::getPoseFromPPFCorresspondence(PointType& model_point, NormalType& model_normal, PointType& scene_point, NormalType&scene_normal, float alpha, Eigen::Affine3f& transformation)
{

	Eigen::Vector3f model_p = model_point.getVector3fMap(),
		scene_p = scene_point.getVector3fMap(),
		model_n = model_normal.getNormalVector3fMap(),
		scene_n = scene_normal.getNormalVector3fMap();

	//model to global
	bool parallel_to_x_mg = (model_n.y() > -FLT_EPSILON && model_n.y()<FLT_EPSILON && model_n.z()>-FLT_EPSILON && model_n.z() < FLT_EPSILON);
	Eigen::Vector3f rotation_axis_mg = (parallel_to_x_mg) ? (Eigen::Vector3f::UnitY()) : (model_n.cross(Eigen::Vector3f::UnitX()).normalized());
	Eigen::AngleAxisf rotation_mg(acosf(model_n.dot(Eigen::Vector3f::UnitX())), rotation_axis_mg);
	Eigen::Affine3f transform_mg(Eigen::Translation3f(rotation_mg * ((-1) * model_p)) * rotation_mg);
	//scene to global
	bool parallel_to_x_sg = (scene_n.y() > -FLT_EPSILON && scene_n.y()<FLT_EPSILON && scene_n.z()>-FLT_EPSILON && scene_n.z() < FLT_EPSILON);
	Eigen::Vector3f rotation_axis_sg = (parallel_to_x_sg) ? (Eigen::Vector3f::UnitY()) : (scene_n.cross(Eigen::Vector3f::UnitX()).normalized());
	Eigen::AngleAxisf rotation_sg(acosf(scene_n.dot(Eigen::Vector3f::UnitX())), rotation_axis_sg);
	Eigen::Affine3f transform_sg(Eigen::Translation3f(rotation_sg * ((-1) * scene_p)) * rotation_sg);
	//total
	transformation = Eigen::Affine3f(transform_sg.inverse()*Eigen::AngleAxisf(alpha, Eigen::Vector3f::UnitX())*transform_mg);

	//@todo is it necessary?
	//Eigen::AngleAxisf tmp1(transform_ms.rotation());
	//Eigen::Vector3f rot = tmp1.angle()*tmp1.axis();
	//Eigen::Vector3f trans = transform_ms.translation();
	//if (!pcl_isfinite(trans(0) || !pcl_isfinite(trans(1)) || !pcl_isfinite(trans(2)) || !pcl_isfinite(rot(0)) || !pcl_isfinite(rot(1)) || !pcl_isfinite(rot(2)))) {
	//	PCL_WARN("ERROR!!!!!");
	//	return false;
	//}
	return true;
}


void zyk::PPF_Space::ICP_Refine(pcl::PointCloud<PointType>::Ptr scene, const vector<zyk::pose_cluster, Eigen::aligned_allocator<zyk::pose_cluster>>& coarse_pose_clusters, vector<zyk::pose_cluster, Eigen::aligned_allocator<zyk::pose_cluster>>& pose_clusters_out, int max_number, double scene_res, double max_dis)
{
	std::cout << "Start ICP..." << std::endl;
	int time1 = clock();
	if (max_number <= 0)
		max_number = 10;
	max_number = std::min(max_number, int(coarse_pose_clusters.size()));
	
	if (max_dis < 0) {
		//auto calculate the max_dis;
		if (scene_res < 0) {
			// scene_res is not provided, should calculate the scene resolution
			scene_res = computeCloudResolution(scene);
		}
		max_dis = scene_res*1.414 + 0.05;
		//std::cout << "ICP max distance computed: " << max_dis << std::endl;
	}
	pcl::PointCloud<PointType>::Ptr rotated_model(new pcl::PointCloud<PointType>());
	pcl::PointCloud<PointType>::Ptr icp_res(new pcl::PointCloud<PointType>());
	pcl::IterativeClosestPoint<PointType, PointType> icp;
	icp.setMaximumIterations(100);
	icp.setInputTarget(scene);
	icp.setTransformationEpsilon(1e-10);
	icp.setEuclideanFitnessEpsilon(0.001);
	icp.setMaxCorrespondenceDistance(max_dis);
	for (size_t i = 0; i < max_number; ++i) {
		pcl::transformPointCloud(*input_point_cloud, *rotated_model, coarse_pose_clusters[i].mean_transformation);
		icp.setInputSource(rotated_model);
		icp.align(*icp_res);
		//calculate icp scores
		double score = 0;
		double thresh = max_dis*max_dis;
		for (int j = 0; j < icp.correspondences_->size(); ++j)
		{
			if (icp.correspondences_->at(j).distance <= thresh)
			{
				score = score + 1.0;
			}
		}
		score /= centered_point_cloud->size();
		pose_clusters_out.push_back(zyk::pose_cluster(Eigen::Affine3f(icp.getFinalTransformation())*coarse_pose_clusters[i].mean_transformation, score));
	}
	double time_spent = (clock() - time1) / 1000.0;
	std::cout << "ICP finished for " << max_number << " in " << time_spent << "s" << std::endl;
	//std::sort(pose_clusters_out.begin(), pose_clusters_out.end(), zyk::pose_cluster_comp);
}

void zyk::PPF_Space::ICP_Refine2_0(pcl::PointCloud<PointType>::Ptr scene, const vector<zyk::pose_cluster, Eigen::aligned_allocator<zyk::pose_cluster>> &coarse_pose_clusters, vector<zyk::pose_cluster, Eigen::aligned_allocator<zyk::pose_cluster>> &pose_clusters_out, int max_number, double scene_res /*= -1.0*/, double max_dis /*= -1.0*/)
{
	std::cout << "Start ICP..." << std::endl;
	int time1 = clock();
	if (max_number <= 0)
		max_number = 10;
	max_number = std::min(max_number, int(coarse_pose_clusters.size()));
	
	//auto calculate the max_dis;
	if (scene_res < 0) {
		// scene_res is not provided, should calculate the scene resolution
		scene_res = computeCloudResolution(scene);
	}
	float final_dis = scene_res*1.414 + 0.05;
	//std::cout << "ICP max distance computed: " << final_dis << std::endl;
	if (max_dis < 0) {
		max_dis = 4 * max_dis;
	}
	pcl::PointCloud<PointType>::Ptr rotated_model(new pcl::PointCloud<PointType>());
	pcl::PointCloud<PointType>::Ptr icp_res(new pcl::PointCloud<PointType>());
	pcl::IterativeClosestPoint<PointType, PointType> icp;
	icp.setMaximumIterations(100);
	icp.setInputTarget(scene);
	icp.setTransformationEpsilon(1e-10);
	icp.setEuclideanFitnessEpsilon(0.001);
	for (size_t i = 0; i < max_number; ++i) {
		pcl::transformPointCloud(*input_point_cloud, *rotated_model, coarse_pose_clusters[i].mean_transformation);
		icp.setMaxCorrespondenceDistance(max_dis);
		icp.setInputSource(rotated_model);
		icp.align(*icp_res);
		icp.setMaxCorrespondenceDistance(final_dis);
		icp_res->clear();
		icp.align(*icp_res,icp.getFinalTransformation());
		//calculate icp scores
		double score = 0;
		double thresh = final_dis*final_dis;
		for (int j = 0; j < icp.correspondences_->size(); ++j)
		{
			if (icp.correspondences_->at(j).distance <= thresh)
			{
				score = score + 1.0;
			}
		}
		score /= centered_point_cloud->size();
		pose_clusters_out.push_back(zyk::pose_cluster(Eigen::Affine3f(icp.getFinalTransformation())*coarse_pose_clusters[i].mean_transformation, score));
	}
	double time_spent = (clock() - time1) / 1000.0;
	std::cout << "ICP finished for " << max_number << " in " << time_spent << "s" << std::endl;
}

bool zyk::PPF_Space::computeAllPPF()
{
	 //Mofified 2018-3-3 by zyk, no need to use centered_point_cloud to compute all ppf, input_point_cloud is same
	//check
	if (input_point_cloud == NULL || input_point_normal == NULL)
		return false;
	if (input_point_cloud->empty() || input_point_normal->empty())
		return false;
	if (input_point_cloud->size() != input_point_normal->size())
		return false;

	//loop
	 for (int i = 0; i < input_point_cloud->size(); i++)
	{
		/*if (model_x_centrosymmetric)
		{
			if (centered_point_cloud->at(i).z > point_cloud_center(2))
				continue;
		}
		if (model_y_centrosymmetric)
		{
			if (centered_point_cloud->at(i).x > point_cloud_center(0))
				continue;
		}
		if (model_z_centrosymmetric)
		{
			if (centered_point_cloud->at(i).y > point_cloud_center(1))
				continue;
		}*/
		 for (int j = 0; j < input_point_cloud->size(); j++)
		{
			if (i != j)
			{
				PPF ppf;
				computeSinglePPF(input_point_cloud, input_point_normal, i, j, ppf);
				if (ignore_plane_switch)
				{
					if (abs(ppf.ppf.f3) < 0.01 && abs(ppf.ppf.f1-M_PI_2)<0.01&&abs(ppf.ppf.f2-M_PI_2)<0.01)
						continue;
				}
				ppf.ppf.alpha_m = computeAlpha(input_point_cloud->at(i).getVector3fMap(), input_point_normal->at(i).getNormalVector3fMap(), input_point_cloud->at(j).getVector3fMap());
				if (!pcl_isfinite(ppf.ppf.alpha_m))
					continue;
				ppf_vector.push_back(ppf);
			}
		}
	}

  return true;
}
#ifdef view_based
bool PPF_Space::computeALL_Visible_PPF(std::vector<std::vector<int> > &view_based_indexes)
{
  //check
  if (input_point_cloud == NULL || input_point_normal == NULL)
    return false;
  if (input_point_cloud->empty() || input_point_normal->empty())
    return false;
  if (input_point_cloud->size() != input_point_normal->size())
    return false;
  if(view_based_indexes.size()<10)
    return false;
  //keep a map to store index pair
  std::set<std::pair<int,int> > pair_set;
  //init the occlusion vector
  occlusion_weights=std::vector<float>(input_point_cloud->size(),0.0);
  float inc=1.0/view_based_indexes.size();
  for(int i=0;i<view_based_indexes.size();++i){
    for(int j=0;j<view_based_indexes[i].size();++j){
      int cur_ref_idx=view_based_indexes[i][j];
      occlusion_weights[cur_ref_idx]+=inc;
      for(int k=0;k<view_based_indexes[i].size();++k){
        int scd_idx=view_based_indexes[i][k];
        if(cur_ref_idx==scd_idx)continue;
        if(pair_set.find(std::pair<int,int>(cur_ref_idx,scd_idx))!=pair_set.end())
        {
          continue;
        }
        else
        {
          pair_set.insert(std::pair<int,int>(cur_ref_idx,scd_idx));
          PPF ppf;
          computeSinglePPF(input_point_cloud, input_point_normal, cur_ref_idx, scd_idx, ppf);
          if (ignore_plane_switch)
          {
            if (abs(ppf.ppf.f3) < 0.01 && abs(ppf.ppf.f1-M_PI_2)<0.01&&abs(ppf.ppf.f2-M_PI_2)<0.01)
              continue;
          }
          ppf.ppf.alpha_m = computeAlpha(input_point_cloud->at(cur_ref_idx).getVector3fMap(), input_point_normal->at(cur_ref_idx).getNormalVector3fMap(), input_point_cloud->at(scd_idx).getVector3fMap());
          if (!pcl_isfinite(ppf.ppf.alpha_m))
            continue;
          ppf_vector.push_back(ppf);
        }
      }
    }
  }
  return true;
}
#endif
void zyk::PPF_Space::getppfBoxCoord(PPF& ppf, Eigen::Vector4i& ijk)
{
	if (!pcl_isfinite(ppf.ppf.f1) || !pcl_isfinite(ppf.ppf.f2) || !pcl_isfinite(ppf.ppf.f3) || !pcl_isfinite(ppf.ppf.f4) || !pcl_isfinite(ppf.ppf.alpha_m))
	{
		ijk[0] = ijk[1] = ijk[2] = ijk[3] = -1;
	}
	//float a1 = (ppf.ppf.f1-min_p[0])*inv_leaf_size(0);
	//float a2 = (ppf.ppf.f2-min_p[1])*inv_leaf_size(1);
	//float a3 = (ppf.ppf.f3-min_p[2])*inv_leaf_size(2);
	//float a4 = (ppf.ppf.f4-min_p[3])*inv_leaf_size(3);
	Eigen::Array4f tmp = (Eigen::Array4f(ppf.ppf.f1, ppf.ppf.f2, ppf.ppf.f3, ppf.ppf.f4) - min_p)*inv_leaf_size;
	//ijk[0] = floor(a1);
	//ijk[1] = floor(a2);
	//ijk[2] = floor(a3);
	//ijk[3] = floor(a4);
	ijk = Eigen::Vector4i(floor(tmp(0)), floor(tmp(1)), floor(tmp(2)), floor(tmp(3)));
	if (ijk[0] < 0 || ijk[1] < 0 || ijk[2] < 0 || ijk[3] < 0 || ijk[0] >= grid_f1_div || ijk[1] >= grid_f2_div || ijk[2] >= grid_f3_div || ijk[3] >= grid_f4_div)
		ijk[0] = ijk[1] = ijk[2] = ijk[3] = -1;
}
void zyk::PPF_Space::getppfBoxCoord(PPF& ppf, int* ijk)
{
	if (!pcl_isfinite(ppf.ppf.f1) || !pcl_isfinite(ppf.ppf.f2) || !pcl_isfinite(ppf.ppf.f3) || !pcl_isfinite(ppf.ppf.f4) || !pcl_isfinite(ppf.ppf.alpha_m))
	{
		ijk[0] = ijk[1] = ijk[2] = ijk[3] = -1;
	}
	//MODIFIED 17-10-22 min_p is not needed
	//float a1 = (ppf.ppf.f1-min_p[0])*inv_leaf_size(0);
	//float a2 = (ppf.ppf.f2-min_p[1])*inv_leaf_size(1);
	//float a3 = (ppf.ppf.f3-min_p[2])*inv_leaf_size(2);
	//float a4 = (ppf.ppf.f4-min_p[3])*inv_leaf_size(3);
	float a1 = (ppf.ppf.f1)*inv_leaf_size(0);
	float a2 = (ppf.ppf.f2)*inv_leaf_size(1);
	float a3 = (ppf.ppf.f3)*inv_leaf_size(2);
	float a4 = (ppf.ppf.f4)*inv_leaf_size(3);
	//Eigen::Array4f tmp = (Eigen::Array4f(ppf.ppf.f1, ppf.ppf.f2, ppf.ppf.f3, ppf.ppf.f4) - min_p)*inv_leaf_size;
	ijk[0] = floor(a1);
	ijk[1] = floor(a2);
	ijk[2] = floor(a3);
	ijk[3] = floor(a4);
	//ijk = Eigen::Vector4i(floor(tmp(0)), floor(tmp(1)), floor(tmp(2)), floor(tmp(3)));
	if (ijk[0] < 0 || ijk[1] < 0 || ijk[2] < 0 || ijk[3]<0 || ijk[0] >= grid_f1_div || ijk[1] >= grid_f2_div || ijk[2] >= grid_f3_div||ijk[3]>=grid_f4_div)
		ijk[0] = ijk[1] = ijk[2] = ijk[3] = -1;
}

int zyk::PPF_Space::getppfBoxIndex(PPF& ppf)
{
#ifdef use_eigen
	Eigen::Vector4i ijk;
#else
	int ijk[4];
#endif
	getppfBoxCoord(ppf, ijk);
#ifdef use_eigen
	int i = ijk.dot(grid_div_mul);
#else
	int i = ijk[0] * grid_div_mul(0) + ijk[1] * grid_div_mul[1] + ijk[2] * grid_div_mul[2] + ijk[3] * grid_div_mul[3];
#endif
	if (i >= total_box_num || i < 0)
		i = -1;
	return i;
}

void zyk::PPF_Space::getNeighboringPPFBoxIndex(int currentIndex, vector<int>&out_vec)
{
	int x3 = currentIndex / grid_div_mul(3);
	currentIndex -= x3*grid_div_mul(3);
	int x2 = currentIndex / grid_div_mul(2);
	currentIndex -= x2*grid_div_mul(2);
	int x1 = currentIndex / grid_div_mul(1);
	currentIndex -= x1*grid_div_mul(1);
	int x0 = currentIndex;

	Eigen::Vector4i coord(x0, x1, x2, x3);
	for (int i = -1; i < 2; ++i)
	{
		if (x0 + i < 0 || x0 + i >= grid_f1_div)
			continue;
		for (int j = -1; j < 2; ++j)
		{
			if (x1 + j < 0 || x1 + j >= grid_f2_div)
				continue;
			for (int k = -1; k < 2; ++k)
			{
				if (x2 + k < 0 || x2 + k >= grid_f3_div)
					continue;
				for (int l = -1; l < 2; ++l)
				{
					if (x3 + l < 0 || x3 + l >= grid_f4_div)
						continue;
					out_vec.push_back((coord + Eigen::Vector4i(i, j, k, l)).dot(grid_div_mul));

				}
			}
		}
	}

}
//for test only
float zyk::PPF_Space::computeClusterScore(pcl::PointCloud<PointType>::Ptr& scene, pcl::PointCloud<NormalType>::Ptr& scene_normals, float dis_thresh, float ang_thresh, zyk::pose_cluster &pose_clusters)
{
	pcl::PointCloud<PointType>::Ptr rotated_model(new pcl::PointCloud<PointType>());
	pcl::PointCloud<NormalType>::Ptr rotated_normal(new pcl::PointCloud<NormalType>());

	CVoxel_grid scene_grid;
	scene_grid.Init(dis_thresh, dis_thresh, dis_thresh, scene);
	//scene_grid.resplit(dis_thresh, dis_thresh, dis_thresh);
	Eigen::Vector3i grid_div;
	scene_grid.getGridDiv(grid_div);
	vector<zyk::box*>* box_vector = scene_grid.getBox_vector();
	if (ang_thresh < 0)ang_thresh = M_PI_2;
	float cos_ang_thresh = cos(ang_thresh);

	float score = 0;
	pcl::transformPointCloud(*centered_point_cloud, *rotated_model, pose_clusters.mean_transformation);
	transformNormals(*input_point_normal, *rotated_normal, pose_clusters.mean_transformation);
	for (int i = 0; i < rotated_model->size(); ++i)
	{
		int pnt_box_index = scene_grid.getPntBoxIndex(rotated_model->at(i));
		if (pnt_box_index == -1)continue;
		//zyk::box* p_box = box_vector->at(pnt_box_index);
		//if (p_box == NULL)continue;
		PointType mp = rotated_model->at(i);
		NormalType mn = rotated_normal->at(i);

		/////////get neighboring
		vector<int>neiboringBoxIndexVector;
		neiboringBoxIndexVector.reserve(30);
		//for convenience, push back current index too!
		neiboringBoxIndexVector.push_back(pnt_box_index);
		zyk::getNeiboringBoxIndex3D(pnt_box_index, grid_div, neiboringBoxIndexVector);

		bool found = false;
		for (int j = 0; j < neiboringBoxIndexVector.size() && !found; ++j)
		{
			int neiboringBoxIndex = neiboringBoxIndexVector[j];
			zyk::box*p_current_neiboring_point_box = box_vector->at(neiboringBoxIndex);
			if (p_current_neiboring_point_box == NULL)
				continue;
			for (int k = 0; k < p_current_neiboring_point_box->size(); ++k)
			{
				PointType sp = scene->at((*p_current_neiboring_point_box)[k]);
				NormalType sn = scene_normals->at((*p_current_neiboring_point_box)[k]);
				if (fabs(mp.x - sp.x) + fabs(mp.y - sp.y) + fabs(mp.z - sp.z) < dis_thresh)
				{
					if (zyk::dot(mn.normal, sn.normal, 3) > cos_ang_thresh) {
						score = score + 1;
						found = true;
						break;
					}
				}
			}
		}

	}
	return score/ centered_point_cloud->size();
}

//void zyk::PPF_Space::Serialize(CArchive &ar)
//{
//	CObject::Serialize(ar);
//	if (ar.IsStoring())
//	{
//		ar << grid_f1_div << grid_f2_div << grid_f3_div << grid_f4_div << min_p(0) << min_p(1) << min_p(2) << min_p(3) << max_p(0) << max_p(1) << max_p(2) << max_p(3)<<ppf_vector.size();
//		for (int i = 0; i < ppf_vector.size(); i++)
//		{
//			ar << ppf_vector[i].first_index << ppf_vector[i].second_index << ppf_vector[i].ppf.alpha_m << ppf_vector[i].ppf.f1 << ppf_vector[i].ppf.f2 << ppf_vector[i].ppf.f3 << ppf_vector[i].ppf.f4;
//		}
//		ar << centered_point_cloud->size();
//		for (int i = 0; i < centered_point_cloud->size(); i++)
//		{
//			ar << centered_point_cloud->at(i).x << centered_point_cloud->at(i).y << centered_point_cloud->at(i).z << input_point_normal->at(i).normal_x << input_point_normal->at(i).normal_y << input_point_normal->at(i).normal_z;
//		}
//	}
//	else
//	{
//		int ppf_vector_size = 0;
//		ar >> grid_f1_div >> grid_f2_div >> grid_f3_div >> grid_f4_div >> min_p(0) >> min_p(1) >> min_p(2) >> min_p(3) >> max_p(0) >> max_p(1) >> max_p(2) >> max_p(3) >> ppf_vector_size;
//		ppf_vector.resize(ppf_vector_size);
//		for (int i = 0; i < ppf_vector_size; i++)
//		{
//			ar >> ppf_vector[i].first_index >> ppf_vector[i].second_index >> ppf_vector[i].ppf.alpha_m >> ppf_vector[i].ppf.f1 >> ppf_vector[i].ppf.f2 >> ppf_vector[i].ppf.f3 >> ppf_vector[i].ppf.f4;
//		}
//		int points_num = 0;
//		ar >> points_num;
//		centered_point_cloud = pcl::PointCloud<PointType>::Ptr(new pcl::PointCloud<PointType>());
//		input_point_normal = pcl::PointCloud<NormalType>::Ptr(new pcl::PointCloud<NormalType>());
//		for (int i = 0; i < points_num; i++)
//		{
//			PointType _tem;
//			NormalType _nor;
//			ar >> _tem.x >> _tem.y >> _tem.z >> _nor.normal_x >> _nor.normal_y >> _nor.normal_z;
//			centered_point_cloud->push_back(_tem);
//			input_point_normal->push_back(_nor);
//		}
//		grid_div_mul(0) = 1;
//		grid_div_mul(1) = grid_div_mul(0)*grid_f1_div;
//		grid_div_mul(2) = grid_div_mul(1)*grid_f2_div;
//		grid_div_mul(3) = grid_div_mul(2)*grid_f3_div;
//		total_box_num = grid_div_mul(3)*grid_f4_div;
//		assert(total_box_num > 0);
//
//		Eigen::Array4f dim = max_p - min_p;
//		leaf_size(0) = dim(0) / grid_f1_div;
//		leaf_size(1) = dim(1) / grid_f2_div;
//		leaf_size(2) = dim(2) / grid_f3_div;
//		leaf_size(3) = dim(3) / grid_f4_div;
//
//		inv_leaf_size(0) = 1 / leaf_size(0);
//		inv_leaf_size(1) = 1 / leaf_size(1);
//		inv_leaf_size(2) = 1 / leaf_size(2);
//		inv_leaf_size(3) = 1 / leaf_size(3);
//
//		//grid
//		ppf_box_vector.resize(total_box_num);
//		constructGrid();
//	}
//}





void zyk::PPF_Space::match(pcl::PointCloud<PointType>::Ptr scene, pcl::PointCloud<NormalType>::Ptr scene_normals,bool spread_ppf_switch, bool two_ball_switch, float relativeReferencePointsNumber,float max_vote_thresh, float max_vote_percentage, int n_angles, float angle_thresh, float first_dis_thresh, float recompute_score_dis_thresh, float recompute_score_ang_thresh, float second_dis_thresh, int num_clusters_per_group, vector<zyk::pose_cluster, Eigen::aligned_allocator<zyk::pose_cluster>>& pose_clusters)
{
	std::cout << "match object: " << mName << std::endl;
	int scene_steps = floor(1.0 / relativeReferencePointsNumber);
	if (scene_steps < 1)scene_steps = 1;

	float diameter = max_p[3];
	//use Going further with ppf's two ball voting scheme
	float small_diameter = 0.8*diameter;
	if (small_diameter > model_size[1])
		small_diameter = model_size[1];
	float box_radius = sqrt(model_size[0] * model_size[0] + model_size[1] * model_size[1] + model_size[2] * model_size[2]);
	first_dis_thresh *= box_radius;
	second_dis_thresh *= box_radius;
	//cout << "Second distance thresh is: " << second_dis_thresh << endl;
	recompute_score_dis_thresh *= box_radius;
	//build grid to accelerate matching process
	CVoxel_grid scene_grid;
	scene_grid.Init(diameter, diameter, diameter, scene);
	Eigen::Vector3i grid_div;
	scene_grid.getGridDiv(grid_div);
	vector<zyk::box*>*box_vector = scene_grid.getBox_vector();
	Eigen::Vector3i div_mul(1, grid_div(0), grid_div(0)*grid_div(1));


	int num_poses_count = 0;
	//all raw poses
	vector<zyk::pose_cluster, Eigen::aligned_allocator<zyk::pose_cluster>> rawClusters;

	///////// info
	int match_count = 0;
	int match_percent = 0;
	int info_step = 0.05*scene->size();

	int tst_cnt1 = 0;
	int tst_cnt2 = 0;
    int tst_cnt3 = 0;//record plane reference point number;
    int tst_cnt4 = 0;//record all plane ppf number
    int tst_cnt5 = 0;//record number of poses that plane feature generate
	///////// vote flag, See Going further with ppf
#ifdef vote_flag_use_map
	unordered_map<int, int>vote_flag;
#else
	vector<int>vote_flag(ppf_box_vector.size(), 0);
#endif
	//init some vectors before loop
	vector<int>neiboringBoxIndexVector;
	neiboringBoxIndexVector.reserve(50);
	vector<int>neighboring_ppf_box_index_vec;
	neighboring_ppf_box_index_vec.reserve(100);
	/////////////////////begin iterate boxes
	for (int box_index = 0; box_index < box_vector->size(); ++box_index)
	{
		/////////get box ptr
		zyk::box*p_current_point_box = box_vector->at(box_index);
		if (p_current_point_box == NULL)
			continue;
		/////////get neighboring
		neiboringBoxIndexVector.clear();
		//for convenience, push back current index too!
		neiboringBoxIndexVector.push_back(box_index);
		zyk::getNeiboringBoxIndex3D(box_index, grid_div, neiboringBoxIndexVector);
		
		for (int cnt1 = 0; cnt1 < box_vector->at(box_index)->size(); cnt1 = cnt1 + scene_steps)
		{
			if ((match_count+=scene_steps) > info_step)
			{
				match_count = 0;
				match_percent += 5;
				cout << "Match : " << match_percent << "%." << endl;
//				cout << "tst_cnt1: " << tst_cnt1 << endl;
//				cout << "tst_cunt2: " << tst_cnt2 << endl;
//				tst_cnt1 = 0;
//				tst_cnt2 = 0;
			}
			int reference_pnt_index = (*p_current_point_box)[cnt1];
			/////////build an accumulator for this reference point
			// in matlab the row size is size+1, why?
			zyk::PPF_Accumulator small_acum(centered_point_cloud->size(), n_angles);
            zyk::PPF_Accumulator big_acum(centered_point_cloud->size(), n_angles);
#ifdef plane_check
            // 2018-3-14 by zyk add plane checker
            int plane_checker=0;
#endif
#ifdef use_eigen
			const Eigen::Vector3f& rp = scene->at(reference_pnt_index).getVector3fMap();
			const Eigen::Vector3f& rn = scene_normals->at(reference_pnt_index).getNormalVector3fMap();
#else
			const PointType& rp = scene->at(reference_pnt_index);
			const NormalType& rn = scene_normals->at(reference_pnt_index);
#endif
#ifdef vote_flag_use_map
			vote_flag.clear();
#else
			vote_flag.assign(ppf_vector.size(), 0);
#endif
			// loop through neighboring boxes to get scene pnt
			for (int cnt2 = 0; cnt2 < neiboringBoxIndexVector.size(); ++cnt2)
			{
				int neiboringBoxIndex = neiboringBoxIndexVector[cnt2];
				zyk::box*p_current_neiboring_point_box = box_vector->at(neiboringBoxIndex);
				if (p_current_neiboring_point_box == NULL)
					continue;
				for (int cnt3 = 0; cnt3 < p_current_neiboring_point_box->size(); ++cnt3)
				{
					int scene_pnt_index = (*p_current_neiboring_point_box)[cnt3];
					if (scene_pnt_index == reference_pnt_index)
						continue;
#ifdef use_eigen
					const Eigen::Vector3f& sp = scene->at(scene_pnt_index).getVector3fMap();
					const Eigen::Vector3f& sn = scene_normals->at(scene_pnt_index).getNormalVector3fMap();
					float distance = (sp-rp).norm();
#else
					const PointType& sp = scene->at(scene_pnt_index);
					const NormalType& sn = scene_normals->at(scene_pnt_index);
					float distance = zyk::dist(sp.data, rp.data, 3);
#endif
					
					if (neiboringBoxIndex != box_index)
					{
#ifdef use_eigen
						if ((sp - rp).norm() > diameter)
							continue;
#else
						if (distance>diameter)
							continue;
#endif
					}
					//calculate ppf
					zyk::PPF current_ppf;
					zyk::PPF_Space::computeSinglePPF(rp, rn, sp, sn, current_ppf);
                    if (abs(current_ppf.ppf.f3) < 0.035 && abs(current_ppf.ppf.f1 - M_PI_2) < 0.035 && abs(current_ppf.ppf.f2 - M_PI_2) < 0.035){
#ifdef plane_check
                        plane_checker++;
#endif
                        tst_cnt4++;
						continue;
                    }
					//do hash
					int ppf_box_index = getppfBoxIndex(current_ppf);
					if (ppf_box_index == -1)
						continue;
					
					//now calculate alpha of current_ppf
					double current_alpha = computeAlpha(rp, rn, sp);
					//18-1-16, bug on pcl 1.8.0 normal compute
					if (!pcl_isfinite(current_alpha))
						continue;

					//Check and set flag, 2018-2-3, zyk, bug fixed, change the following
					//int scene_rotation_discretized = floor((current_ppf.ppf.alpha_m + M_PI) / 2 / M_PI * 32);
					int scene_rotation_discretized = floor((current_alpha + M_PI) / 2 / M_PI * 32);
					if (vote_flag[ppf_box_index] & (1 << scene_rotation_discretized))
						continue;
					else
						vote_flag[ppf_box_index] |= 1 << scene_rotation_discretized;

					neighboring_ppf_box_index_vec.clear();
					neighboring_ppf_box_index_vec.push_back(ppf_box_index);
					if(spread_ppf_switch)
						getNeighboringPPFBoxIndex(ppf_box_index, neighboring_ppf_box_index_vec);
					for (int cnt4 = 0; cnt4 < neighboring_ppf_box_index_vec.size(); ++cnt4)
					{
						zyk::box* current_ppf_box = ppf_box_vector.at(neighboring_ppf_box_index_vec[cnt4]);
						if (current_ppf_box == NULL)
							continue;
						//loop hash list
						for (int node = 0; node < current_ppf_box->size(); ++node)
						{
							zyk::PPF& correspond_model_ppf = ppf_vector.at((*current_ppf_box)[node]);
							float alpha = correspond_model_ppf.ppf.alpha_m - current_alpha;
							if (alpha >= M_PI) alpha -= 2 * M_PI;
							if (alpha < -M_PI)alpha += 2 * M_PI;
							float alpha_bin = (alpha + M_PI) / 2 / M_PI * n_angles;
							int middle_bin = int(alpha_bin + 0.5f);
							if (middle_bin >= n_angles) middle_bin = 0;
							//calculate vote_val
							//float tmp = cos(correspond_model_ppf.ppf.f3);
							//if (1 - tmp < 0.003)
							//{
							//	continue;
							//}
							//float vote_val = 1 - 0.98 * fabs(cos(correspond_model_ppf.ppf.f3));
							if (two_ball_switch) {
								if (distance < small_diameter)
									small_acum.acumulator(correspond_model_ppf.first_index, middle_bin) += correspond_model_ppf.weight;
								else
									big_acum.acumulator(correspond_model_ppf.first_index, middle_bin) += correspond_model_ppf.weight;
							}
							else
							{
								small_acum.acumulator(correspond_model_ppf.first_index, middle_bin) += correspond_model_ppf.weight;
							}

							tst_cnt2++;
						}

					}
					

				}

			}//end of loop neiboring boxes
			//Now check the accumulator and extract poses
			Eigen::MatrixXi::Index maxRow, maxCol;
			float maxVote_small = small_acum.acumulator.maxCoeff(&maxRow, &maxCol);
			float maxVote_big = 0;
			if (two_ball_switch) {
				big_acum.acumulator += small_acum.acumulator;
				maxVote_big = big_acum.acumulator.maxCoeff(&maxRow, &maxCol);
			}
			//if (max_vote_thresh > 0){
			if (maxVote_small < max_vote_thresh)
				continue;
			//}
			float vote_thresh_small = max_vote_percentage*maxVote_small;
			float vote_thresh_big = max_vote_percentage*maxVote_big;
			//visit the accumulator and extract poses
			for (int row = 0; row < small_acum.acumulator.rows(); ++row)
			{
				for (int col = 0; col < small_acum.acumulator.cols(); ++col)
				{
					if ((small_acum.acumulator(row, col) > vote_thresh_small) || (two_ball_switch&&big_acum.acumulator(row, col) > vote_thresh_big))
					{
#ifdef plane_check
                        if(!plane_flag.empty()){
                            if(plane_checker>plane_vote_thresh){
                                tst_cnt3++;
                                if(!plane_flag[row])
                                    continue;
                            }
                            else{
                                if(plane_flag[row])
                                    continue;
                            }
                        }
#endif
						num_poses_count++;
						tst_cnt1++;
						float alpha = col / 30.0 * 2 * M_PI - M_PI;
						Eigen::Affine3f transformation;
						if (!zyk::PPF_Space::getPoseFromPPFCorresspondence(centered_point_cloud->at(row), input_point_normal->at(row), scene->at(reference_pnt_index), scene_normals->at(reference_pnt_index), alpha, transformation))
							continue;

						rawClusters.push_back(zyk::pose_cluster(transformation, small_acum.acumulator(row, col)));
					}

				}
			}//end of visit accumulator

		}

	}
#ifdef plane_check
    std::cout<<"All plane points: "<<tst_cnt3<<std::endl;
    std::cout<<"All plane features: "<<tst_cnt4<<std::endl;
    std::cout<<"Num of poses plane features: "<<tst_cnt5<<std::endl;
#endif
	std::sort(rawClusters.begin(), rawClusters.end(), zyk::pose_cluster_comp);
	cout << "all poses number : " << rawClusters.size() << endl;
	////////////Now do clustering
	// raw cluster first
    if(first_dis_thresh<0)//only for test
    {
        for (int i = 0; i < rawClusters.size(); i++)
        {
            int result = 0;
            for (int j = 0; j < pose_clusters.size(); j++)
            {
                if (pose_clusters[j].checkAndPutIn(rawClusters[i].transformations[0], rawClusters[i].vote_count, first_dis_thresh, angle_thresh))
                {
                    //if(++result>=max_clusters_per_pose_can_be_in)
                    result = 1;
                    break;
                }
            }
            if (!result)
                pose_clusters.push_back(rawClusters[i]);
        }
    }
    else
        pose_clusters=rawClusters;
	std::sort(pose_clusters.begin(), pose_clusters.end(), zyk::pose_cluster_comp);


	//////recompute score
	if (recompute_score_dis_thresh > 0)
	{
		//cout << "Now recompute scores, distance thresh is: " << recompute_score_dis_thresh << endl;
		recomputeClusterScore(scene_grid,*scene_normals, recompute_score_dis_thresh, recompute_score_ang_thresh, pose_clusters);
		std::sort(pose_clusters.begin(), pose_clusters.end(), zyk::pose_cluster_comp);
	}

	// second cluster by distance
	// group those close clusters together and rank them
	if (num_clusters_per_group > 0){
		cout << "Now do grouping, number of clusters per group is : " << num_clusters_per_group << endl;
		vector<vector<pose_cluster, Eigen::aligned_allocator<zyk::pose_cluster>>> groups;
		for (int i = 0; i < pose_clusters.size(); i++)
		{
			int result = 0;
			for (int j = 0; j < groups.size(); j++)
			{
				if ((groups[j][0].mean_trans - pose_clusters[i].mean_trans).norm() < second_dis_thresh)
				{
					groups[j].push_back(pose_clusters[i]);
					result = 1;
					break;
				}
			}
			if (!result)
			{
				vector<pose_cluster, Eigen::aligned_allocator<zyk::pose_cluster>> tmp_clusters;
				tmp_clusters.push_back(pose_clusters[i]);
				groups.push_back(tmp_clusters);
			}
		}

		cout << "group size is: " << groups.size() << endl;
		pose_clusters.clear();
		for (int i = 0; i < groups.size(); i++)
		{
			for (int j = 0; j < min(num_clusters_per_group, int(groups[i].size())); ++j)
			{
				pose_clusters.push_back(groups[i][j]);
			}		
		}
	}
	for (size_t i = 0; i < pose_clusters.size(); ++i) {
		zyk::pose_cluster& pos = pose_clusters[i];
		pos.mean_transformation.translate(Eigen::Vector3f(-cx, -cy, -cz));
		pos.mean_trans = pos.mean_transformation.translation();
	}
}


void zyk::PPF_Space::recomputeClusterScore(zyk::CVoxel_grid& grid, pcl::PointCloud<NormalType>& scene_normals, float dis_thresh, float ang_thresh, vector<zyk::pose_cluster, Eigen::aligned_allocator<zyk::pose_cluster>> &pose_clusters)
{
	
	pcl::PointCloud<PointType>::Ptr rotated_model(new pcl::PointCloud<PointType>());
	pcl::PointCloud<NormalType>::Ptr rotated_normal(new pcl::PointCloud<NormalType>());
	pcl::PointCloud<PointType>::Ptr scene = grid.getInputPointCloud();

	grid.resplit(dis_thresh , dis_thresh , dis_thresh );
	Eigen::Vector3i grid_div;
	grid.getGridDiv(grid_div);

	vector<zyk::box*>* box_vector = grid.getBox_vector();
	if (ang_thresh < 0)ang_thresh = M_PI_2;
	float cos_ang_thresh = cos(ang_thresh);

#ifdef use_neiboringIterator
	zyk::NeiboringIterator niter(grid_div.data(), 3);
#else // use_neiboringIterator
	vector<int>neiboringBoxIndexVector;
	neiboringBoxIndexVector.reserve(30);
#endif
	for (int num = 0; num < pose_clusters.size(); ++num)
	{
		float score = 0;
		pcl::transformPointCloud(*centered_point_cloud, *rotated_model, pose_clusters[num].mean_transformation);
		transformNormals(*input_point_normal, *rotated_normal, pose_clusters[num].mean_transformation);
		for (int i = 0; i < rotated_model->size(); ++i)
		{
			int pnt_box_index = grid.getPntBoxIndex(rotated_model->at(i));
			if (pnt_box_index == -1)continue;
			//zyk::box* p_box = box_vector->at(pnt_box_index);
			//if (p_box == NULL)continue;
			PointType mp = rotated_model->at(i);
			NormalType mn = rotated_normal->at(i);

			/////////get neighboring
			//for convenience, push back current index too!
#ifdef use_neiboringIterator
			niter.setTarget(pnt_box_index);
#else // use_neiboringIterator
			neiboringBoxIndexVector.clear();
			neiboringBoxIndexVector.push_back(pnt_box_index);
			zyk::getNeiboringBoxIndex3D(pnt_box_index, grid_div, neiboringBoxIndexVector);
#endif
			bool found = false;
#ifdef use_neiboringIterator
			for (; !niter.isDone() && !found; ++niter) 
			{
				int neiboringBoxIndex = niter.getIndex();

#else
			for (int j = 0; j < neiboringBoxIndexVector.size() && !found; ++j)
			{
				int neiboringBoxIndex = neiboringBoxIndexVector[j];
#endif
				zyk::box*p_current_neiboring_point_box = box_vector->at(neiboringBoxIndex);
				if (p_current_neiboring_point_box == NULL)
					continue;
				for (int k = 0; k < p_current_neiboring_point_box->size(); ++k)
				{
					PointType sp = scene->at((*p_current_neiboring_point_box)[k]);
					NormalType sn = scene_normals.at((*p_current_neiboring_point_box)[k]);
					if (fabs(mp.x - sp.x) + fabs(mp.y - sp.y) + fabs(mp.z - sp.z) < dis_thresh)
					{
						if (zyk::dot(mn.normal, sn.normal, 3) > cos_ang_thresh) {
							score = score + 1;
							found = true;
							break;
						}
					}
				}
			}

		}
		pose_clusters[num].old_vote_count = pose_clusters[num].vote_count;
		pose_clusters[num].vote_count = score / centered_point_cloud->size();
	}
}

bool zyk::PPF_Space::save(std::string file_name)
{
	bool res = false;
	std::ofstream ofs(file_name, std::ios::binary);
	if (ofs.is_open())
	{
		boost::archive::binary_oarchive ar(ofs);
		this->save(ar, 1);
		res = true;
	}
	ofs.close();
	return res;
}

bool zyk::PPF_Space::load(std::string fine_name)
{
	bool res = false;
	std::ifstream ifs(fine_name, std::ios::binary);
	if (ifs.is_open())
	{
		boost::archive::binary_iarchive ar(ifs);
		this->load(ar, 1);
		res = true;
	}
	ifs.close();
	return res;

}
