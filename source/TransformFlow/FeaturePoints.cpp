//
//  FeaturePoints.cpp
//  Transform Flow
//
//  Created by Samuel Williams on 1/03/12.
//  Copyright (c) 2012 Orion Transfer Ltd. All rights reserved.
//

#include "FeaturePoints.h"
#include <Dream/Events/Logger.h>
#include <Euclid/Geometry/AlignedBox.h>

namespace TransformFlow {
	
	using namespace Dream::Events::Logging;
	using namespace Euclid::Geometry;

	template <typename F>
	static inline void bresenham_normalized_line(Vec2i start, Vec2i end, F callback)
	{
		bool steep = abs(end[Y] - start[Y]) > abs(end[X] - start[X]);
		
		if (steep) {
			std::swap(start[X], start[Y]);
			std::swap(end[X], end[Y]);
		}
		
		if (start[X] > end[X]) {
			std::swap(start[X], end[X]);
			std::swap(start[Y], end[Y]);
		}
		
		int dx = end[X] - start[X];
		int dy = abs(end[Y] - start[Y]);
		int error = dx / 2;
		
		int ystep = -1;
		int y = start[Y];
		
		if (start[Y] < end[Y])
			ystep = 1;
		
		for (int x = start[X]; x < end[X]; x += 1) {
			if (steep) {
				callback({y, x});
			} else {
				callback({x, y});
			}
			
			// Calculate the next step
			error = error - dy;
			
			if (error < 0) {
				y = y + ystep;
				error = error + dx;
			}
		}
	}

	template <typename F>
	static inline void bresenham_ordered_line(Vec2i start, Vec2i end, F callback)
	{
		bool steep = abs(end[Y] - start[Y]) > abs(end[X] - start[X]);
		
		if (steep) {
			std::swap(start[X], start[Y]);
			std::swap(end[X], end[Y]);
		}
		
		int dx = abs(end[X] - start[X]);
		int dy = abs(end[Y] - start[Y]);
		int error = dx / 2;

		int y = start[Y];

		int ystep = start[Y] < end[Y] ? 1 : -1;
		int increment = start[X] < end[X] ? 1 : -1;
		
		for (int x = start[X]; x != end[X]; x += increment) {
			if (steep) {
				callback({y, x});
			} else {
				callback({x, y});
			}
			
			// Calculate the next step
			error = error - dy;
			
			if (error < 0) {
				y = y + ystep;
				error = error + dx;
			}
		}
	}

	template <typename NumericT, std::size_t H = 5>
	struct LaplacianGradients
	{
		inline NumericT laplace(const NumericT values[H], const std::size_t offset)
		{
			auto mid = (((H-1)/2 + offset) % H);
			NumericT sum = values[mid] * (H-1);

			// Subtractions and modulus don't work as you'd expect..
			mid = mid + H;

			for (std::size_t i = 1; i <= (H-1)/2; i += 1)
			{
				sum += values[(mid-i) % H] * -1;
				sum += values[(mid+i) % H] * -1;
			}

			return sum;
		}

		NumericT input[H];
		std::size_t k = 0;

		NumericT output[2];

		template <typename F>
		inline void sum(const NumericT & value, F callback) {
			input[k % H] = value;

			if (k >= (H-1)) {
				// Latest measurement is stored in output[1], previous measurement stored in output[0].
				output[1] = laplace(input, (k - (H-1)) % H);

				if (k >= H) {
					callback(k - (H-1)/2);
				}

				output[0] = output[1];
			}

			k += 1;
		}
		
		std::size_t index() const
		{
			return k % H;
		}
		
		NumericT & at(std::size_t i) { return input[i % H]; }
		const NumericT & at(std::size_t i) const { return input[i % H]; }

		RealT variance_min_max(const std::size_t & index) const
		{
			RealT min = 255.0, max = 0.0;

			for (int i = -2; i <= 2; i += 1) {
				min = std::min(min, at(i+index));
				max = std::max(max, at(i+index));
			}

			return (max - min);
		}

		RealT variance_left_right(const std::size_t & index) const
		{
			auto ia = (at(index-2) + at(index-1)) / 2.0;
			auto ib = at(index);
			auto ic = (at(index+2) + at(index+1)) / 2.0;

			auto dab = (ib - ia), dbc = (ic - ib);
			auto d = (dab*dab) + (dbc*dbc);

			return d;
		}

		RealT variance_min(const std::size_t & index) const
		{
			auto ia = (at(index-2) + at(index-1)) / 2.0;
			auto ic = (at(index+2) + at(index+1)) / 2.0;

			auto dac = (ic - ia);

			return fabsf(dac);
		}

		RealT variance_edge_to_edge(const std::size_t & index) const
		{
			const int K = (H-1) / 2;

			// Normal pixel difference
			auto ia = at(index-K);
			auto ic = at(index+K);

			auto dac = (ic - ia);

			return fabsf(dac);
		}

		bool good_edge(const std::size_t & index) const
		{
			//return variance_min(index) > 30;
			//return variance_edge_to_edge(index) > 20;
			
			// Found this to be the most robust:
			return variance_left_right(index) >= 600;
		}
	};

	template <typename NumericT>
	NumericT midpoint(const NumericT & a, const NumericT b)
	{
		//return 0.5;
		return -a / (b-a);
		//return -b / (a-b);
	}

	void FeaturePoints::features_along_line(Ptr<Image> image, Vec2i start, Vec2i end, std::vector<Vec2> & features) {
		//AlignedBox2 image_box = AlignedBox2::from_origin_and_size(ZERO, image->size());

		// We want the algorithm to work with the origin in the bottom left, not the top left.
		start[Y] = (int)image->size()[HEIGHT] - start[Y];
		end[Y] = (int)image->size()[HEIGHT] - end[Y];

		const std::size_t H = 5;
		typedef Vector<3, unsigned char> PixelT;
		LaplacianGradients<RealT, H> gradients;
		
		auto image_reader = reader(*image);

		Vec2 offsets[H];

		bresenham_normalized_line(start, end, [&](const Vec2i & offset) {
			//assert(offset.greater_than_or_equal({0, 0}));
			//assert(offset.less_than(image->size()));

			RealT intensity = Vec3(PixelT(image_reader[offset])).sum() / 3.0;
			//log_debug("pixel", Vec3(image_reader[offset]), intensity);

			Vec2 image_offset = offset;
			image_offset[Y] = image->size()[HEIGHT] - image_offset[Y];

			offsets[gradients.index()] = image_offset;
			
			gradients.sum(intensity, [&](std::size_t index) {
				auto & a = gradients.output[0];
				auto & b = gradients.output[1]; // index
				//auto d = (b - a);

				// Early break out:
				//if ((d*d) < 100) return;

				if (a != 0 && b == 0)
				{
					if (gradients.good_edge(index) == false) return;

					//assert(image_box.intersects_with(offsets[index % H]));
					
					// Zero crossing at index (very rare).
					features.push_back(offsets[index % H]);
				}
				else if ((a < 0 && b > 0) || (b < 0 && a > 0))
				{
					if (gradients.good_edge(index) == false) return;

					// Midpoint between index-1 and index.
					auto m = linear_interpolate<RealT>(midpoint(a, b), offsets[(index-1) % H], offsets[index % H]);
					
					//assert(image_box.intersects_with(m));
					
					features.push_back(m);
				} else {
					return;
				}
			});
		});
	}

	FeaturePoints::FeaturePoints() {
		
	}
	
	FeaturePoints::~FeaturePoints() {
		
	}

	void FeaturePoints::scan(Ptr<Image> source, const Radians<> & tilt, std::size_t dy)
	{
		if (_offsets.size()) return;
		
		_source = source;
		AlignedBox2 image_box(ZERO, _source->size());

		{
			AlignedBox2 bounds = ZERO;

			// Forward rotation, create a bounding box where -y is "down".
			Mat22 rotation = rotate<Z>(tilt);

			Vec2 size = _source->size();

			bounds.union_with_point(rotation * size);
			bounds.union_with_point(rotation * Vec2(size[X], 0));
			bounds.union_with_point(rotation * Vec2(0, size[Y]));

			_bounding_box = bounds;
		}

		{
			// Now we need to enumerate lines in the "rotated" space, and translate them back to image space:
			Mat22 rotation = rotate<Z>(-tilt);

			AlignedBox2 clipping_box = AlignedBox2::from_center_and_size(image_box.center(), image_box.size() * 0.98);

			//std::max<std::size_t>(_bounding_box.size()[HEIGHT] / 100, 2);

			for (auto y = _bounding_box.min()[Y] + dy; (y + dy) < _bounding_box.max()[Y]; y += dy) {
				Vec2 min(_bounding_box.min()[X], y), max(_bounding_box.max()[X], y);

				// This segment is now in image space, perpendicular to gravity.
				LineSegment2 segment(rotation * min, rotation * max), clipped_segment;
				//_segments.push_back(segment);

				//log_debug("Features along line segment:", segment.start(), segment.end(), segment.direction());

				if (segment.clip(clipping_box, clipped_segment)) {
					//log_debug("Clipped line segment:", segment.start(), segment.end(), segment.direction());
					
					//DREAM_ASSERT(clipped_segment.direction().equivalent(segment.direction()));

					_segments.push_back(clipped_segment);

					Vec2 start = clipped_segment.start();
					Vec2 end = clipped_segment.end();

					features_along_line(source, start, end, _offsets);
				}
			}
		}

		_table = new FeatureTable(dy, 2, image_box, tilt);

		_table->update(_offsets);

		//log_debug("Found", _offsets.size(), "feature points.");
	}
}
