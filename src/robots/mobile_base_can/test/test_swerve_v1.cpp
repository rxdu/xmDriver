/*
 * Swerve reference profile: id mapping into the model-state extension range,
 * round-trip, reject paths, and stable profile identity (spec §8, profile v1).
 *
 * Copyright (c) 2026 Ruixiang Du (rdu). SPDX-License-Identifier: Apache-2.0
 */
#include "mobile_base_can/profiles/swerve_v1.hpp"

#include <gtest/gtest.h>

using namespace xmotion;
using namespace xmotion::mobile_base;
namespace sw = xmotion::mobile_base::profiles::swerve_v1;

TEST(MobileBaseSwerveV1, IdMappingInExtensionRange) {
  for (std::uint8_t i = 0; i < sw::kNumModules; ++i) {
    EXPECT_EQ(sw::ModuleId(i), 0x220u + i);
    EXPECT_TRUE(IsModelStateId(sw::ModuleId(i)));
  }
}

TEST(MobileBaseSwerveV1, ModuleRoundTrip) {
  for (std::uint8_t i = 0; i < 4; ++i) {
    sw::ModuleState s{i, (i % 2 ? -0.6 : 0.6), 0.25, 0x00,
                      static_cast<std::uint8_t>(i)};
    auto d = sw::Decode(sw::Encode(s));
    ASSERT_TRUE(d.has_value());
    EXPECT_EQ(d->index, i);
    EXPECT_NEAR(d->speed, s.speed, 1e-3);
    EXPECT_NEAR(d->angle, 0.25, 1e-3);
    EXPECT_EQ(d->counter, i);
  }
}

TEST(MobileBaseSwerveV1, RejectPaths) {
  {  // CRC corruption
    CanFrame f = sw::Encode(sw::ModuleState{0, 0.5, 0.1, 0, 1});
    f.data[2] ^= 0x01;
    EXPECT_FALSE(sw::Decode(f).has_value());
  }
  {  // id outside the module range
    CanFrame f = sw::Encode(sw::ModuleState{0, 0.5, 0.1, 0, 1});
    f.id = 0x224;  // one past module 3
    EXPECT_FALSE(sw::Decode(f).has_value());
  }
}

TEST(MobileBaseSwerveV1, StableProfileIdentity) {
  // Independent of the core protocol version — evolving the profile never
  // touches the core.
  EXPECT_EQ(sw::kProfileId, 1);
  EXPECT_EQ(sw::kProfileVersion, 1);
}
