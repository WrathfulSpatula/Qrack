//////////////////////////////////////////////////////////////////////////////////////
//
// (C) Daniel Strano and the Qrack contributors 2017-2019. All rights reserved.
//
// This class allows access to on-chip RNG capabilities. The class is adapted from these two sources:
// https://codereview.stackexchange.com/questions/147656/checking-if-cpu-supports-rdrand/150230
// https://stackoverflow.com/questions/45460146/how-to-use-intels-rdrand-using-inline-assembly-with-net
//
// Licensed under the GNU Lesser General Public License V3.
// See LICENSE.md in the project root or https://www.gnu.org/licenses/lgpl-3.0.en.html
// for details.

#pragma once

#if ENABLE_RDRAND

#if _MSC_VER
#include <intrin.h>
#else
#include <cpuid.h>
#endif

#include <immintrin.h>
#endif

#if ENABLE_ANURAND
#include "AnuRandom.hpp"
#endif
#include "qrack_types.hpp"

namespace Qrack {

bool getRdRand(unsigned int* pv);

class RdRandom {
public:
#if ENABLE_ANURAND
    RdRandom()
        : didInit(false)
        , isPageTwo(false)
        , dataOffset(0){};
#endif

    bool SupportsRDRAND();
    real1 Next();

#if ENABLE_ANURAND
private:
    bool didInit;
    bool isPageTwo;
    AnuRandom::Data data1;
    AnuRandom::Data data2;
    int dataOffset;
#endif
};
} // namespace Qrack
