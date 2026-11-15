#include <gtest/gtest.h>

#include <string>

#include "mixxxtest.h"
#include "preferences/configobject.h"
#include "stems/demucsonnxseparator.h"
#include "stems/stemgpudetect.h"

namespace mixxx::stems {

namespace {
const ConfigKey kGpuAccelerationConfigKey("[Stems]", kGpuAccelerationKey);
} // namespace

class StemGpuDetectTest : public MixxxTest {
  protected:
    bool rawValue(bool fallback) const {
        return config()->getValue(kGpuAccelerationConfigKey, fallback);
    }
};

TEST_F(StemGpuDetectTest, ExplicitValueWins) {
    // Whatever the machine's default is, an explicit value of the opposite
    // kind must be stored and returned.
    const bool opposite = !gpuAccelerationDefault();
    setGpuAccelerationEnabled(config(), opposite);
    EXPECT_TRUE(config()->exists(kGpuAccelerationConfigKey));
    EXPECT_EQ(isGpuAccelerationEnabled(config()), opposite);
    EXPECT_EQ(rawValue(!opposite), opposite);
}

TEST_F(StemGpuDetectTest, StoringTheDefaultRemovesTheKey) {
    // A value equal to the detected default is not stored, so a later hardware
    // change can still move the default.
    setGpuAccelerationEnabled(config(), !gpuAccelerationDefault());
    ASSERT_TRUE(config()->exists(kGpuAccelerationConfigKey));
    setGpuAccelerationEnabled(config(), gpuAccelerationDefault());
    EXPECT_FALSE(config()->exists(kGpuAccelerationConfigKey));
    // With no stored key the answer is the detected default again.
    EXPECT_EQ(isGpuAccelerationEnabled(config()), gpuAccelerationDefault());
}

TEST_F(StemGpuDetectTest, MissingKeyUsesDetectedDefault) {
    ASSERT_FALSE(config()->exists(kGpuAccelerationConfigKey));
    EXPECT_EQ(isGpuAccelerationEnabled(config()), gpuAccelerationDefault());
}

TEST_F(StemGpuDetectTest, DefaultRequiresGpuAndProvider) {
    if (gpuAccelerationDefault()) {
        EXPECT_TRUE(hasDedicatedGpu());
        EXPECT_TRUE(hasGpuInferenceProvider());
    } else {
        EXPECT_TRUE(!hasDedicatedGpu() || !hasGpuInferenceProvider());
    }
}

TEST_F(StemGpuDetectTest, DeviceRequest) {
    EXPECT_EQ(stemDeviceForGpuAcceleration(true), std::string("auto"));
    EXPECT_EQ(stemDeviceForGpuAcceleration(false), std::string("cpu"));
}

} // namespace mixxx::stems
