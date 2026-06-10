#include <gtest/gtest.h>

#include "PetDecayEngine.h"
#include "PetEvolution.h"
#include "PetState.h"

using pet::PetConfig;
using pet::PetStage;
using pet::PetState;

TEST(PetDecay, ZeroHoursIsNoOp) {
  PetState s;
  s.hunger = 50;
  s.happiness = 50;
  s.health = 50;
  pet::applyDecay(s, 0, 0);
  EXPECT_EQ(s.hunger, 50);
  EXPECT_EQ(s.happiness, 50);
  EXPECT_EQ(s.health, 50);
}

TEST(PetDecay, HungerAndHappinessDecayPerHour) {
  PetState s;
  s.hunger = 80;
  s.happiness = 80;
  s.health = 100;
  pet::applyDecay(s, 2, 0);
  EXPECT_EQ(s.hunger, 80 - 2 * PetConfig::HUNGER_DECAY_PER_HOUR);
  EXPECT_EQ(s.happiness, 80 - 2 * PetConfig::HAPPINESS_DECAY_PER_HOUR);
  // Health only decays once hunger hits 0.
  EXPECT_EQ(s.health, 100);
}

TEST(PetDecay, HealthDecaysOnlyWhenStarving) {
  PetState s;
  s.hunger = 4;  // one hour of HUNGER_DECAY (4) lands exactly on 0
  s.health = 100;
  pet::applyDecay(s, 1, 0);
  EXPECT_EQ(s.hunger, 0);
  // hunger reaches 0 this tick, so health decays by HEALTH_DECAY_PER_HOUR.
  EXPECT_EQ(s.health, 100 - PetConfig::HEALTH_DECAY_PER_HOUR);
}

TEST(PetDecay, ClampsToZeroNeverWraps) {
  PetState s;
  s.hunger = 5;
  s.happiness = 5;
  pet::applyDecay(s, 100, 0);
  EXPECT_EQ(s.hunger, 0);
  EXPECT_EQ(s.happiness, 0);
}

TEST(PetDecay, CapsElapsedAt720Hours) {
  PetState s;
  s.hunger = 100;
  // 1,000,000 hours would underflow without the cap; result must still be a
  // clean 0 (cap then clamp), not a wrapped value.
  pet::applyDecay(s, 1000000, 0);
  EXPECT_EQ(s.hunger, 0);
}

TEST(PetEvolution, EggUntilFirstGate) {
  PetState s;
  s.stage = PetStage::EGG;
  EXPECT_EQ(pet::computeStage(s, 0, 0), PetStage::EGG);
  EXPECT_EQ(pet::computeStage(s, 1, 19), PetStage::EGG);   // pages short
  EXPECT_EQ(pet::computeStage(s, 0, 20), PetStage::EGG);   // days short
}

TEST(PetEvolution, ReachesHatchlingAtGate) {
  PetState s;
  s.stage = PetStage::EGG;
  EXPECT_EQ(pet::computeStage(s, 1, 20), PetStage::HATCHLING);
}

TEST(PetEvolution, ReachesEachStageAtItsGate) {
  PetState s;
  s.stage = PetStage::EGG;
  EXPECT_EQ(pet::computeStage(s, 3, 100), PetStage::YOUNGSTER);
  EXPECT_EQ(pet::computeStage(s, 7, 500), PetStage::COMPANION);
  EXPECT_EQ(pet::computeStage(s, 14, 1500), PetStage::ELDER);
}

TEST(PetEvolution, IsOneWay) {
  PetState s;
  s.stage = PetStage::COMPANION;
  // Even with no qualifying age/pages, never regress below recorded stage.
  EXPECT_EQ(pet::computeStage(s, 0, 0), PetStage::COMPANION);
}

TEST(PetEvolution, MaybeEvolveReportsAdvance) {
  PetState s;
  s.stage = PetStage::EGG;
  EXPECT_TRUE(pet::maybeEvolve(s, 1, 20));
  EXPECT_EQ(s.stage, PetStage::HATCHLING);
  EXPECT_FALSE(pet::maybeEvolve(s, 1, 20));  // already there
}
