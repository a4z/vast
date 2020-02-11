/******************************************************************************
 *                    _   _____   __________                                  *
 *                   | | / / _ | / __/_  __/     Visibility                   *
 *                   | |/ / __ |_\ \  / /          Across                     *
 *                   |___/_/ |_/___/ /_/       Space and Time                 *
 *                                                                            *
 * This file is part of VAST. It is subject to the license terms in the       *
 * LICENSE file found in the top-level directory of this distribution and at  *
 * http://vast.io/license. No part of VAST, including this file, may be       *
 * copied, modified, propagated, or distributed except according to the terms *
 * contained in the LICENSE file.                                             *
 ******************************************************************************/

#include "vast/segment.hpp"

#include "vast/bitmap.hpp"
#include "vast/bitmap_algorithms.hpp"
#include "vast/detail/assert.hpp"
#include "vast/detail/byte_swap.hpp"
#include "vast/detail/narrow.hpp"
#include "vast/error.hpp"
#include "vast/fbs/segment.hpp"
#include "vast/fbs/utils.hpp"
#include "vast/filesystem.hpp"
#include "vast/ids.hpp"
#include "vast/logger.hpp"
#include "vast/si_literals.hpp"
#include "vast/table_slice.hpp"

#include <caf/binary_deserializer.hpp>

namespace vast {

using namespace binary_byte_literals;

segment_ptr segment::make(chunk_ptr chunk) {
  VAST_ASSERT(chunk != nullptr);
  // Verify flatbuffer integrity.
  auto data = reinterpret_cast<const uint8_t*>(chunk->data());
  auto verifier = flatbuffers::Verifier{data, chunk->size()};
  if (!fbs::VerifySegmentBuffer(verifier)) {
    VAST_ERROR_ANON(__func__, "failed to verify segment buffer");
    return nullptr;
  }
  // Perform version check.
  auto ptr = fbs::GetSegment(chunk->data());
  if (ptr->version() != fbs::SegmentVersion::v1) {
    VAST_ERROR_ANON(__func__, "unsupported segment version");
    return nullptr;
  }
  return segment_ptr{new segment{std::move(chunk)}, false};
}

uuid segment::id() const {
  auto ptr = fbs::GetSegment(chunk_->data());
  auto data = span<const uint8_t, 16>(ptr->uuid()->Data(), 16);
  return uuid{as_bytes(data)};
}
vast::ids segment::ids() const {
  vast::ids result;
  auto ptr = fbs::GetSegment(chunk_->data());
  for (auto flat_slice : *ptr->data()) {
    result.append_bits(false, flat_slice->offset() - result.size());
    result.append_bits(true, flat_slice->rows());
  }
  return result;
}

size_t segment::num_slices() const {
  return fbs::GetSegment(chunk_->data())->data()->size();
}

chunk_ptr segment::chunk() const {
  return chunk_;
}

caf::expected<std::vector<table_slice_ptr>>
segment::lookup(const vast::ids& xs) const {
  std::vector<table_slice_ptr> result;
  auto f = [](auto flat_slice) {
    return std::pair{flat_slice->offset(),
                     flat_slice->offset() + flat_slice->rows()};
  };
  auto g = [&](auto flat_slice) -> caf::error {
    // TODO: bind the lifetime of the table slice to the segment chunk.
    result.push_back(fbs::make_table_slice(*flat_slice));
    return caf::none;
  };
  auto ptr = fbs::GetSegment(chunk_->data());
#if 0
  auto verifier = flatbuffers::Verifier{
    reinterpret_cast<const uint8_t*>(chunk_->data()), chunk_->size()};
  VAST_ASSERT(fbs::VerifySegmentBuffer(verifier));
#endif
  auto begin = ptr->data()->begin();
  auto end = ptr->data()->end();
  if (auto error = select_with(xs, begin, end, f, g))
    return error;
  return result;
}
caf::error inspect(caf::serializer& sink, const segment_ptr& x) {
  VAST_ASSERT(x != nullptr);
  return sink(x->chunk_);
}

caf::error inspect(caf::deserializer& source, segment_ptr& x) {
  x.reset(new segment, false);
  return source(x->chunk_);
}

caf::error save(const path& filename, const segment_ptr& x) {
  VAST_ASSERT(x != nullptr);
  return write(filename, x->chunk());
}

caf::error load(const path& filename, segment_ptr& x) {
  auto chk = chunk::mmap(filename);
  if (!chk)
    return make_error(ec::filesystem_error, "failed to mmap chunk", filename);
  x = segment::make(chk);
  if (!x)
    return make_error(ec::unspecified, "failed to construct segment");
  return caf::none;
}

segment::segment(chunk_ptr chk) : chunk_{std::move(chk)} {
}

} // namespace vast
