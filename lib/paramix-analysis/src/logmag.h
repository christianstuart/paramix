#pragma once

namespace paramix::analysis {

/// log10(1 + v) through the decoded bit-level polynomial:
/// t = 1+v, exponent stripped to 1.0, y = mantissa-1,
/// q = (0.40526167 - y*0.10423073)*y, r = (exp-127)*0.30103001.
float log10p1(float value);

/// Four-lane sum over the first `count` entries with stride 4, then a
/// sequential lane fold. Matches the native 80-bin flux accumulation.
float sumLanes4(const float* values, int count);

} // namespace paramix::analysis
