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

#include "rdrandwrapper.hpp"

namespace Qrack {

bool getRdRand(unsigned int* pv)
{
#if ENABLE_RDRAND
    const int max_rdrand_tries = 10;
    for (int i = 0; i < max_rdrand_tries; ++i) {
        if (_rdrand32_step(pv))
            return true;
    }
#endif
    return false;
}

bool RdRandom::SupportsRDRAND()
{
#if ENABLE_RDRAND
    const unsigned int flag_RDRAND = (1 << 30);

#if _MSC_VER
    int ex[4];
    __cpuid(ex, 1);

    return ((ex[2] & flag_RDRAND) == flag_RDRAND);
#else
    unsigned int eax, ebx, ecx, edx;
    ecx = 0;
    __get_cpuid(1, &eax, &ebx, &ecx, &edx);

    return ((ecx & flag_RDRAND) == flag_RDRAND);
#endif

#elif ENABLE_ANURAND
    return true;
#else
    return false;
#endif
}

real1 RdRandom::Next()
{
#if ENABLE_RDRAND
    unsigned int v;
    real1 res = 0;
    real1 part = 1;
    if (!getRdRand(&v)) {
        throw "Failed to get hardware RNG number.";
    }
    v &= 0x7fffffff;
    for (int i = 0; i < 31; i++) {
        part /= 2;
        if (v & (1U << i)) {
            res += part;
        }
    }
    return res;
#elif ENABLE_ANURAND
    if (!didInit) {
        AnuRandom rnd;
        data1 = rnd.read();
        data2 = rnd.read();
        didInit = true;
    }
    if ((isPageTwo && (data2.size() - dataOffset) < 4) || (!isPageTwo && (data1.size() - dataOffset) < 4)) {
        AnuRandom rnd;
        if (isPageTwo) {
            data1 = rnd.read();
        } else {
            data2 = rnd.read();
        }
        isPageTwo = !isPageTwo;
        dataOffset = 0;
    }
    for (int i = 0; i < 4; i++) {
        part /= 256;
        res += part * (isPageTwo ? data2[dataOffset + i] : data1[dataOffset + i]);
    }
    dataOffset += 4;
    return res;
#else
    return 0;
#endif
}

} // namespace Qrack
