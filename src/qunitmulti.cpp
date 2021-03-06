//////////////////////////////////////////////////////////////////////////////////////
//
// (C) Daniel Strano and the Qrack contributors 2017, 2018. All rights reserved.
//
// QUnitMulti is a multiprocessor variant of QUnit.
// QUnit maintains explicit separability of qubits as an optimization on a QEngine.
// See https://arxiv.org/abs/1710.05867
// (The makers of Qrack have no affiliation with the authors of that paper.)
//
// Licensed under the GNU Lesser General Public License V3.
// See LICENSE.md in the project root or https://www.gnu.org/licenses/lgpl-3.0.en.html
// for details.

#include "qunitmulti.hpp"
#include "common/oclengine.hpp"
#include "qfactory.hpp"

namespace Qrack {

QUnitMulti::QUnitMulti(bitLenInt qBitCount, bitCapInt initState, qrack_rand_gen_ptr rgp, complex phaseFac, bool doNorm,
    bool randomGlobalPhase, bool useHostMem, int deviceID, bool useHardwareRNG, bool useSparseStateVec,
    real1 norm_thresh, std::vector<bitLenInt> devList)
    : QUnit(QINTERFACE_OPENCL, qBitCount, initState, rgp, phaseFac, doNorm, randomGlobalPhase, useHostMem, -1,
          useHardwareRNG, useSparseStateVec, norm_thresh)
{
    // Notice that this constructor does not take an engine type parameter, and it always passes QINTERFACE_OPENCL to
    // the QUnit constructor. For QUnitMulti, the "shard" engines are therefore guaranteed to always be QEngineOCL
    // types, and it's safe to assume that they can be cast from QInterfacePtr types to QEngineOCLPtr types in this
    // class.

    std::vector<DeviceContextPtr> deviceContext = OCLEngine::Instance()->GetDeviceContextPtrVector();

    if (devList.size() == 0) {
        defaultDeviceID = OCLEngine::Instance()->GetDefaultDeviceID();

        for (bitLenInt i = 0; i < deviceContext.size(); i++) {
            DeviceInfo deviceInfo;
            deviceInfo.id = i;
            deviceList.push_back(deviceInfo);
        }

        std::swap(deviceList[0], deviceList[defaultDeviceID]);
    } else {
        defaultDeviceID = devList[0];

        for (bitLenInt i = 0; i < devList.size(); i++) {
            DeviceInfo deviceInfo;
            deviceInfo.id = devList[i];
            deviceList.push_back(deviceInfo);
        }
    }

    for (bitLenInt i = 0; i < deviceList.size(); i++) {
        deviceList[i].maxSize = deviceContext[deviceList[i].id]->device.getInfo<CL_DEVICE_MAX_MEM_ALLOC_SIZE>();
    }

    if (devList.size() == 0) {
        std::sort(deviceList.begin() + 1, deviceList.end(), std::greater<DeviceInfo>());
    }

    RedistributeSingleQubits();
}

void QUnitMulti::RedistributeSingleQubits()
{
    for (bitLenInt i = 0; i < qubitCount; i++) {
        std::dynamic_pointer_cast<QEngineOCL>(shards[i].unit)
            ->SetDevice(deviceList[(int)((real1)(i * deviceList.size()) / qubitCount)].id);
    }
}

std::vector<QEngineInfo> QUnitMulti::GetQInfos()
{
    // Get shard sizes and devices
    std::vector<QInterfacePtr> qips;
    QEngineOCLPtr qOCL;
    std::vector<QEngineInfo> qinfos;
    int deviceIndex;

    for (auto&& shard : shards) {
        if (std::find(qips.begin(), qips.end(), shard.unit) == qips.end()) {
            qips.push_back(shard.unit);
            qOCL = std::dynamic_pointer_cast<QEngineOCL>(shard.unit);
            deviceIndex = std::distance(deviceList.begin(),
                std::find_if(
                    deviceList.begin(), deviceList.end(), [&](DeviceInfo di) { return di.id == qOCL->GetDeviceID(); }));
            qinfos.push_back(QEngineInfo(qOCL, deviceIndex));
        }
    }

    // We distribute in descending size order:
    std::sort(qinfos.rbegin(), qinfos.rend());

    return qinfos;
}

void QUnitMulti::RedistributeQEngines()
{
    // No need to redistribute, if there is only 1 device
    if (deviceList.size() == 1) {
        return;
    }

    // Get shard sizes and devices
    std::vector<QEngineInfo> qinfos = GetQInfos();

    std::vector<bitCapInt> devSizes(deviceList.size());
    std::fill(devSizes.begin(), devSizes.end(), 0U);
    bitCapInt sz;
    bitLenInt devID, devIndex;
    bitLenInt i, j;

    for (i = 0; i < qinfos.size(); i++) {
        // If the engine adds negligible load, we can let any given unit keep its
        // residency on this device.
        // In fact, single qubit units will be handled entirely by the CPU, anyway.
        if (qinfos[i].unit->GetMaxQPower() <= 2U) {
            continue;
        }

        // If the original OpenCL device has equal load to the least, we prefer the original.
        devID = qinfos[i].unit->GetDeviceID();
        devIndex = qinfos[i].deviceIndex;
        sz = devSizes[devIndex];

        // If the original device has 0 determined load, don't switch the unit.
        if (sz > 0) {
            // If the default OpenCL device has equal load to the least, we prefer the default.
            if (devSizes[0] < sz) {
                devID = deviceList[0].id;
                devIndex = 0;
                sz = devSizes[0];
            }

            // Find the device with the lowest load.
            for (j = 0; j < deviceList.size(); j++) {
                if ((devSizes[j] < sz) && ((devSizes[j] + qinfos[i].unit->GetMaxQPower()) <= deviceList[j].maxSize)) {
                    devID = deviceList[j].id;
                    devIndex = j;
                    sz = devSizes[j];
                }
            }

            // Add this unit to the device with the lowest load.
            qinfos[i].unit->SetDevice(devID);
        }

        // Update the size of buffers handles by this device.
        devSizes[devIndex] += qinfos[i].unit->GetMaxQPower();
    }
}

void QUnitMulti::Detach(bitLenInt start, bitLenInt length, QUnitMultiPtr dest)
{
    QUnit::Detach(start, length, dest);
    RedistributeQEngines();
}

QInterfacePtr QUnitMulti::EntangleInCurrentBasis(
    std::vector<bitLenInt*>::iterator first, std::vector<bitLenInt*>::iterator last)
{
    QEngineOCLPtr unit1 = std::dynamic_pointer_cast<QEngineOCL>(shards[**first].unit);

    // If already fully entangled, just return unit1.
    bool isAlreadyEntangled = true;
    for (auto bit = first + 1; bit < last; bit++) {
        QInterfacePtr unit = shards[**bit].unit;
        if (unit1 != unit) {
            isAlreadyEntangled = false;
            break;
        }
    }

    // This does nothing if the first unit is the default device:
    if (!isAlreadyEntangled && deviceList[0].id != unit1->GetDeviceID()) {
        // Check if size exceeds single device capacity:
        bitLenInt qubitCount = 0;
        std::map<QInterfacePtr, bool> found;

        for (auto bit = first; bit < last; bit++) {
            QInterfacePtr unit = shards[**bit].unit;
            if (found.find(unit) == found.end()) {
                found[unit] = true;
                qubitCount += unit->GetQubitCount();
            }
        }

        // If device capacity is exceeded, put on default device:
        if (pow2(qubitCount) > std::dynamic_pointer_cast<QEngineOCL>(unit1)->GetMaxSize()) {
            unit1->SetDevice(deviceList[0].id);
        }
    }

    QInterfacePtr toRet = QUnit::EntangleInCurrentBasis(first, last);
    if (!isAlreadyEntangled) {
        RedistributeQEngines();
    }
    return toRet;
}

void QUnitMulti::SetPermutation(bitCapInt perm, complex phaseFac)
{
    QUnit::SetPermutation(perm, phaseFac);
    RedistributeSingleQubits();
}

bool QUnitMulti::TrySeparate(bitLenInt start, bitLenInt length)
{
    bool toRet = QUnit::TrySeparate(start, length);
    if (toRet) {
        RedistributeQEngines();
    }

    return toRet;
}

void QUnitMulti::SeparateBit(bool value, bitLenInt qubit, bool doDispose)
{
    QUnit::SeparateBit(value, qubit, doDispose);
    RedistributeQEngines();
}

QInterfacePtr QUnitMulti::Clone()
{
    // TODO: Copy buffers instead of flushing?
    ToPermBasisAll();
    EndAllEmulation();

    QUnitMultiPtr copyPtr = std::make_shared<QUnitMulti>(
        qubitCount, 0, rand_generator, complex(ONE_R1, ZERO_R1), doNormalize, randGlobalPhase, useHostRam);

    return CloneBody(copyPtr);
}

void QUnitMulti::GetQuantumState(complex* outputState)
{
    ToPermBasisAll();
    EndAllEmulation();

    OrderContiguous(EntangleAll());
    shards[0].unit->GetQuantumState(outputState);
}

void QUnitMulti::GetProbs(real1* outputProbs)
{
    ToPermBasisAll();
    EndAllEmulation();

    OrderContiguous(EntangleAll());
    shards[0].unit->GetProbs(outputProbs);
}

} // namespace Qrack
