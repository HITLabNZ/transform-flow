//
//  OpticalFlowMotionModel.cpp
//  File file is part of the "Transform Flow" project and released under the MIT License.
//
//  Created by Samuel Williams on 12/8/2013.
//  Copyright, 2013, by Samuel Williams. All rights reserved.
//

#include "OpticalFlowMotionModel.h"

#include <opencv2/core/core.hpp>
#include <opencv2/features2d/features2d.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <Dream/Events/Logger.h>

#include <ctime>

namespace TransformFlow
{

	using namespace Dream::Events::Logging;

	std::vector<Vec2> find_key_points(Ptr<Image> pixel_buffer)
	{
		std::clock_t start = std::clock();

		Vec3u size = pixel_buffer->size();
		cv::Mat color_frame(size[Y], size[X], CV_8UC4, (void*)pixel_buffer->data());
		cv::Mat greyscale_frame(size[Y], size[X], CV_8UC1);

		cv::cvtColor(color_frame, greyscale_frame, CV_RGB2GRAY);

		//cv::GoodFeaturesToTrackDetector feature_detector(400);
		//cv::FastFeatureDetector feature_detector(20);
		cv::OrbFeatureDetector feature_detector;
		std::vector<cv::KeyPoint> key_points;

		feature_detector.detect(greyscale_frame, key_points);

		//cv::FREAK feature_extractor;
		cv::OrbDescriptorExtractor feature_extractor;
		
		//cv::Mat descriptors;
		//feature_extractor.compute(greyscale_frame, key_points, descriptors);

		std::clock_t end = std::clock();
		double duration = double(end - start) / CLOCKS_PER_SEC;

		//log_debug("Keypoint Detection Time:", duration, "count =", key_points.size());

		std::vector<Vec2> features;
		for (cv::KeyPoint key_point : key_points) {
			features.push_back(Vec2(key_point.pt.x, size[Y] - key_point.pt.y));
		}

		return features;
	}
	
	OpticalFlowMotionModel::OpticalFlowMotionModel() : _image_primed(false)
	{
		_matching_algorithm = matchingAlgorithmUsingORB();
	}
	
	OpticalFlowMotionModel::~OpticalFlowMotionModel()
	{
	}

	void OpticalFlowMotionModel::update(const ImageUpdate & image_update)
	{
		if (_image_primed) {
			auto transform = _matching_algorithm->calculate_local_transform(_image_update, image_update);
		}

		_image_primed = true;
		_image_update = image_update;
	}
}
