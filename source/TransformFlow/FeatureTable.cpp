//
//  FeatureTable.cpp
//  Transform Flow
//
//  Created by Samuel Williams on 23/04/13.
//  Copyright (c) 2013 Orion Transfer Ltd. All rights reserved.
//

#include "FeatureTable.h"
#include "FeaturePoints.h"
#include "Alignment.h"

#include <Euclid/Numerics/Average.h>

namespace TransformFlow {

	using namespace Dream::Events::Logging;

	FeatureTable::FeatureTable(std::size_t bins, const AlignedBox2 & bounds, const Radians<> & rotation) : _bounds(ZERO)
	{
		// The bounds provided are the bounds of the image. Points are image coordinates, but this isn't suitable for binning. We want to bin along the axis perpendicular to gravity, so we make a rotation which does this. We also want to center the table at the origin, so we apply a translation.
		_transform = rotate<Z>(rotation) << translate(-bounds.size() / 2);

		_bins.resize(bins);

		// Calculate a new rotated bounding box:
		_bounds.union_with_point(_transform * bounds.min());
		_bounds.union_with_point(_transform * bounds.max());
		_bounds.union_with_point(_transform * bounds.corner({false, true}));
		_bounds.union_with_point(_transform * bounds.corner({true, false}));
	}

	void FeatureTable::print_table(std::ostream & output)
	{
		std::size_t index = 0;

		for (auto & bin : _bins) {
			output << "Bin " << index << ": ";

			for (auto & chain : bin.links) {
				output << chain.offset << "; ";
			}

			output << std::endl;

			index += 1;
		}
	}

	FeatureTable::Chain * FeatureTable::find_previous_similar(Vec2 aligned_offset, std::size_t index)
	{
		const Vec2 MAX_DISPLACEMENT = {4, 25};

		Chain * best_chain = nullptr;
		Vec2 best_displacement;

		// We scan one bin to the left, and one bin to the right, and find the closest chain.
		auto begin = _bins.begin() + index;
		auto end = _bins.begin() + (index + 1);

		if (begin != _bins.begin()) {
			--begin;
			//log_debug("Looking to left, size =", begin->links->size());
		}

		if (end != _bins.end()) {
			++end;
			//log_debug("Looking to right, size =", (end - 1)->links->size());
		}

		//log_debug("find_previous_similar", offset, " @ ", index);

		for (auto bin = begin; bin != end; ++bin) {
			if (bin->links.size() == 0) continue;

			Chain * previous_chain = &bin->links.back();
			Vec2 displacement = (aligned_offset - previous_chain->aligned_offset).absolute();

			//DREAM_ASSERT(previous_chain->offset[Y] < offset[Y]);

			//log_debug("*", previous_chain->offset, "displacement", displacement);

			// Displacement constants
			if (displacement[X] > MAX_DISPLACEMENT[X] || displacement[Y] > MAX_DISPLACEMENT[Y]) continue;

			if (best_chain == nullptr || displacement.length() < best_displacement.length()) {
				best_chain = previous_chain;
				best_displacement = displacement;
			}
		}

		//if (best_chain)
		//	log_debug("->", best_chain->offset, "displacement", best_displacement);

		return best_chain;
	}

	void FeatureTable::update(const std::vector<Vec2> & offsets)
	{
		for (auto offset : offsets)
		{
			// The aligned offset is the coordinate relative to the origin, with -Z = gravity.
			auto aligned_offset = _transform * offset;

			// We are just concerned with horizontal offset relative to the bounding box:
			auto f = (aligned_offset[X] - _bounds.min()[X]) / _bounds.size()[X];
			DREAM_ASSERT(f >= 0 && f < 1);
			
			// Find the appropriate bin for the given offset:
			std::size_t index = f * _bins.size();

			auto & bin = _bins.at(index);

			Chain * previous_link = find_previous_similar(aligned_offset, index);

			// Add the chain link into the correct bin:
			bin.links.push_back({aligned_offset, offset, nullptr});

			// Add the chain into the list of all chains if it is a new chain:
			if (previous_link != nullptr) {
				previous_link->next = &bin.links.back();
			} else {
				_chains.push_back(&bin.links.back());
			}

			//print_table(std::cout);
		}

		//std::size_t i = 0;
		//for (auto bin : _bins) {
		//	std::cerr << "Bin " << i++ << ": " << bin.links.size() << std::endl;
		//}

		//std::cerr << "Found " << _chains.size() << " chains." << std::endl;
	}

	/*
		Scan through middle of image.
		
		find gradients
		
		based on r radius, scan at approximate gradient edge, and chain together.
		
		build chains of edges.
		
		almagamte into one data structure, chain, with approximate centre, size, and colours. Primarily left or right edge, e.g. tree trunk against white building, trunk is the main part of the edge.
		
		track from one frame to the next by scanning algorithm, adjust size based on edges, perhaps use binary search.
	*/

	Average<RealT> FeatureTable::average_chain_position(std::size_t bin) const
	{
		Average<RealT> distribution;
		
		for (auto & chain : _bins[bin].links)
		{
			//log_debug("Adding chain, aligned_offset =", chain.aligned_offset, "offset =", chain.offset);
			distribution.add_sample(chain.aligned_offset[X]);
		}

		return distribution;
	}

	Average<RealT> FeatureTable::calculate_offset(const FeatureTable & other) const
	{
		return align_tables(*this, other);
	}
};
