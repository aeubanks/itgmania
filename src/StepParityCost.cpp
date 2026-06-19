#include "StepParityCost.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

#include "NoteTypes.h"
#include "StepParityDatastructs.h"

using namespace StepParity;

static const bool VALIDATE_ACTION_COST_CACHE = false;

namespace {
template <typename T>
bool isEmpty(const std::vector<T>& vec, int columnCount) {
  for (int i = 0; i < columnCount; i++) {
    if (static_cast<int>(vec[i]) != 0) {
      return false;
    }
  }
  return true;
}

void hashCombine(uint64_t& seed, uint64_t value) {
  // Diffuse bits with MurmurHash3 fmix64-style finalizer.
  value *= 0xff51afd7ed558ccdULL;
  value ^= value >> 33;
  value *= 0xc4ceb9fe1a85ec53ULL;

  seed ^= value;
  seed = seed * 0x9e3779b97f4a7c15ULL + 0x165667b19e3779f9ULL;
}

void hashCombineFloat(uint64_t& seed, float value) {
  uint32_t bits;
  std::memcpy(&bits, &value, sizeof(bits));
  hashCombine(seed, bits);
}

uint64_t computeRowHash(const Row& row) {
  uint64_t hash = 0;
  hashCombine(hash, row.note_mask);
  hashCombine(hash, row.hold_mask);
  hashCombine(hash, row.mine_mask);
  hashCombine(hash, row.fake_mine_mask);
  hashCombine(hash, static_cast<uint64_t>(row.noteCount));
  for (const IntermediateNoteData& note : row.notes) {
    hashCombine(hash, static_cast<uint64_t>(note.type));
  }
  for (const IntermediateNoteData& hold : row.holds) {
    hashCombine(hash, static_cast<uint64_t>(hold.type));
    if (hold.type != TapNoteType_Empty) {
      hashCombineFloat(hash, hold.beat + hold.hold_length - row.beat);
    }
  }
  return hash;
}

uint64_t hashRow(const Row& row) {
  if (row.actionCostHash == 0) {
    row.actionCostHash = computeRowHash(row);
  }
  return row.actionCostHash;
}

uint64_t packColumns(const FootPlacement& columns) {
  uint64_t packed = 0;
  for (size_t i = 0; i < columns.size(); i++) {
    packed |= (static_cast<uint64_t>(columns[i]) & 0x7) << (3 * i);
  }
  return packed;
}
}  // namespace

float StepParityCost::getActionCost(
    const State* initialState, const State* resultState, const Row& row,
    const Row* previousRow, const FootPlacement& columns, float elapsedTime) {
  uint64_t cacheKey = 0;
  hashCombine(cacheKey, reinterpret_cast<uintptr_t>(initialState));
  hashCombine(cacheKey, reinterpret_cast<uintptr_t>(resultState));
  hashCombine(cacheKey, packColumns(columns));
  hashCombine(cacheKey, hashRow(row));
  hashCombine(cacheKey, previousRow != nullptr ? hashRow(*previousRow) : 0);
  hashCombine(cacheKey, previousRow != nullptr ? 1 : 0);
  hashCombineFloat(
      cacheKey, previousRow != nullptr ? row.beat - previousRow->beat : 0.0f);
  hashCombineFloat(cacheKey, elapsedTime);
  auto cached = actionCostCache.find(cacheKey);
  bool cacheHit = cached != actionCostCache.end();
  if (cacheHit && !VALIDATE_ACTION_COST_CACHE) {
    return cached->second;
  }

  int columnCount = row.columnCount;

  float cost = 0;

  // Mine weighting
  int leftHeel = resultState->whatNoteTheFootIsHitting[LEFT_HEEL];
  int leftToe = resultState->whatNoteTheFootIsHitting[LEFT_TOE];
  int rightHeel = resultState->whatNoteTheFootIsHitting[RIGHT_HEEL];
  int rightToe = resultState->whatNoteTheFootIsHitting[RIGHT_TOE];

  bool movedLeft = resultState->didTheFootMove[LEFT_HEEL] ||
                   resultState->didTheFootMove[LEFT_TOE];

  bool movedRight = resultState->didTheFootMove[RIGHT_HEEL] ||
                    resultState->didTheFootMove[RIGHT_TOE];

  // Note that this is checking whether the previous state was a jump, not
  // whether the current state is
  bool didJump = ((initialState->didTheFootMove[LEFT_HEEL] &&
                   !initialState->isTheFootHolding[LEFT_HEEL]) ||
                  (initialState->didTheFootMove[LEFT_TOE] &&
                   !initialState->isTheFootHolding[LEFT_TOE])) &&
                 ((initialState->didTheFootMove[RIGHT_HEEL] &&
                   !initialState->isTheFootHolding[RIGHT_HEEL]) ||
                  (initialState->didTheFootMove[RIGHT_TOE] &&
                   !initialState->isTheFootHolding[RIGHT_TOE]));

  bool jackedLeft = didJackLeft(
      initialState, resultState, leftHeel, leftToe, movedLeft, didJump);
  bool jackedRight = didJackRight(
      initialState, resultState, rightHeel, rightToe, movedRight, didJump);

  cost += calcMineCost(resultState, row, columnCount);
  cost += calcHoldSwitchCost(initialState, resultState, row, columnCount);
  cost += calcBracketTapCost(
      initialState, row, leftHeel, leftToe, rightHeel, rightToe, elapsedTime);
  cost += calcBracketJackCost(
      resultState, movedLeft, movedRight, jackedLeft, jackedRight, didJump);
  cost += calcDoublestepCost(
      initialState, resultState, row, previousRow, movedLeft, movedRight,
      jackedLeft, jackedRight, didJump);
  cost += calcSlowBracketCost(row, movedLeft, movedRight, elapsedTime);
  cost += calcTwistedFootCost(resultState);
  cost += calcFacingCosts(resultState);
  cost += calcSpinCosts(initialState, resultState);
  cost +=
      calcFootswitchCost(initialState, columns, row, elapsedTime, columnCount);
  cost += calcSideswitchCost(initialState, resultState, columns);
  cost += calcMissedFootswitchCost(row, jackedLeft, jackedRight);
  cost +=
      calcJackCost(movedLeft, movedRight, jackedLeft, jackedRight, elapsedTime);
  cost += calcBigMovementsQuicklyCost(initialState, resultState, elapsedTime);

  if (cacheHit) {
    ASSERT_M(
        cached->second == cost,
        "StepParityCost action cost cache returned a stale result");
    return cost;
  }

  actionCostCache[cacheKey] = cost;
  return cost;
}

// Calculate the cost of avoiding a mine before the current step
// If a mine occurred just before a step, add to the cost
// ex:
// 00M0
// 0010 <- add cost
//
// 00M0
// 0100 <- no cost
float StepParityCost::calcMineCost(
    const State* resultState, const Row& row, int columnCount) {
  if (row.mine_mask == 0 && row.fake_mine_mask == 0) {
    return 0.0f;
  }
  float cost = 0;

  for (int i = 0; i < columnCount; i++) {
    if (resultState->combinedColumns[i] != NONE &&
        (row.mines[i] != 0 || row.fakeMines[i] != 0)) {
      cost += MINE;
      break;
    }
  }
  return cost;
}

// Calculate a cost from having to switch feet in the middle of a hold.
// Multiply the HOLDSWITCH cost by the distance that the "intial" foot
// that was holding the note had to travel to it's new position.
// If the initial foot doesn't move anywhere, then don't mulitply it by
// anything.
float StepParityCost::calcHoldSwitchCost(
    const State* initialState, const State* resultState, const Row& row,
    int columnCount) {
  if (row.hold_mask == 0) {
    return 0.0f;
  }

  float cost = 0;

  for (int c = 0; c < columnCount; c++) {
    if (row.holds[c].type == TapNoteType_Empty) {
      continue;
    }
    if (((resultState->combinedColumns[c] == LEFT_HEEL ||
          resultState->combinedColumns[c] == LEFT_TOE) &&
         initialState->combinedColumns[c] != LEFT_TOE &&
         initialState->combinedColumns[c] != LEFT_HEEL) ||
        ((resultState->combinedColumns[c] == RIGHT_HEEL ||
          resultState->combinedColumns[c] == RIGHT_TOE) &&
         initialState->combinedColumns[c] != RIGHT_TOE &&
         initialState->combinedColumns[c] != RIGHT_HEEL)) {
      int previousFoot =
          initialState->whereTheFeetAre[resultState->combinedColumns[c]];
      cost += HOLDSWITCH * (previousFoot == INVALID_COLUMN
                                ? 1
                                : sqrt(layout->getDistanceSq(c, previousFoot)));
    }
  }
  return cost;
}

// Calculate the cost of tapping a bracket during a hold note
//
// ex:
// 0200
// 0000
// 1000 <- maybe bracketable, if left heel is holding Down arrow
// 0300

float StepParityCost::calcBracketTapCost(
    const State* initialState, const Row& row, int leftHeel, int leftToe,
    int rightHeel, int rightToe, float elapsedTime) {
  if (row.hold_mask == 0) {
    return 0.0f;
  }

  // Small penalty for trying to jack a bracket during a hold
  float cost = 0;
  if (leftHeel != INVALID_COLUMN && leftToe != INVALID_COLUMN) {
    float jackPenalty = 1;
    if (initialState->didTheFootMove[LEFT_HEEL] ||
        initialState->didTheFootMove[LEFT_TOE]) {
      jackPenalty = 1 / elapsedTime;
    }
    if (row.holds[leftHeel].type != TapNoteType_Empty &&
        row.holds[leftToe].type == TapNoteType_Empty) {
      cost += BRACKETTAP * jackPenalty;
    }
    if (row.holds[leftToe].type != TapNoteType_Empty &&
        row.holds[leftHeel].type == TapNoteType_Empty) {
      cost += BRACKETTAP * jackPenalty;
    }
  }

  if (rightHeel != INVALID_COLUMN && rightToe != INVALID_COLUMN) {
    float jackPenalty = 1;
    if (initialState->didTheFootMove[RIGHT_TOE] ||
        initialState->didTheFootMove[RIGHT_HEEL]) {
      jackPenalty = 1 / elapsedTime;
    }

    if (row.holds[rightHeel].type != TapNoteType_Empty &&
        row.holds[rightToe].type == TapNoteType_Empty) {
      cost += BRACKETTAP * jackPenalty;
    }
    if (row.holds[rightToe].type != TapNoteType_Empty &&
        row.holds[rightHeel].type == TapNoteType_Empty) {
      cost += BRACKETTAP * jackPenalty;
    }
  }
  return cost;
}

float StepParityCost::calcBracketJackCost(
    const State* resultState, bool movedLeft, bool movedRight, bool jackedLeft,
    bool jackedRight, bool didJump) {
  if (movedLeft == movedRight || resultState->holding_mask != 0 || didJump) {
    return 0.0f;
  }
  float cost = 0;

  if (jackedLeft && resultState->didTheFootMove[LEFT_HEEL] &&
      resultState->didTheFootMove[LEFT_TOE]) {
    cost += BRACKETJACK;
  }

  if (jackedRight && resultState->didTheFootMove[RIGHT_HEEL] &&
      resultState->didTheFootMove[RIGHT_TOE]) {
    cost += BRACKETJACK;
  }
  return cost;
}

float StepParityCost::calcDoublestepCost(
    const State* initialState, const State* resultState, const Row& row,
    const Row* previousRow, bool movedLeft, bool movedRight, bool jackedLeft,
    bool jackedRight, bool didJump) {
  if ((movedLeft == movedRight) || resultState->holding_mask != 0 || didJump) {
    return 0.0f;
  }

  float cost = 0;
  bool doublestepped = didDoubleStep(
      initialState, row, previousRow, movedLeft, jackedLeft, movedRight,
      jackedRight);

  if (doublestepped) {
    cost += DOUBLESTEP;
  }
  return cost;
}

// Jumps should be prioritized over brackets below a certain speed
float StepParityCost::calcSlowBracketCost(
    const Row& row, bool movedLeft, bool movedRight, float elapsedTime) {
  float cost = 0;
  if (elapsedTime > SLOW_BRACKET_THRESHOLD && movedLeft != movedRight &&
      std::count_if(
          row.notes.begin(), row.notes.end(),
          [](StepParity::IntermediateNoteData note) {
            return note.type != TapNoteType_Empty;
          }) >= 2) {
    float timediff = elapsedTime - SLOW_BRACKET_THRESHOLD;
    cost += timediff * SLOW_BRACKET;
  }
  return cost;
}

// Does this placement result in one of the feet being twisted around?
// This should probably be getting filtered out as an invalid positioning before
// we even get to calculating costs.
float StepParityCost::calcTwistedFootCost(const State* resultState) {
  float cost = 0;
  int leftHeel = resultState->whatNoteTheFootIsHitting[LEFT_HEEL];
  int leftToe = resultState->whatNoteTheFootIsHitting[LEFT_TOE];
  int rightHeel = resultState->whatNoteTheFootIsHitting[RIGHT_HEEL];
  int rightToe = resultState->whatNoteTheFootIsHitting[RIGHT_TOE];

  StagePoint leftPos = layout->averagePoint(leftHeel, leftToe);
  StagePoint rightPos = layout->averagePoint(rightHeel, rightToe);

  bool crossedOver = rightPos.x < leftPos.x;
  bool rightBackwards =
      rightHeel != INVALID_COLUMN && rightToe != INVALID_COLUMN
          ? layout->columns[rightToe].y < layout->columns[rightHeel].y
          : false;
  bool leftBackwards =
      leftHeel != INVALID_COLUMN && leftToe != INVALID_COLUMN
          ? layout->columns[leftToe].y < layout->columns[leftHeel].y
          : false;

  if (!crossedOver && (rightBackwards || leftBackwards)) {
    cost += TWISTED_FOOT;
  }
  return cost;
}

float StepParityCost::calcMissedFootswitchCost(
    const Row& row, bool jackedLeft, bool jackedRight) {
  float cost = 0;
  if ((jackedLeft || jackedRight) &&
      (row.mine_mask != 0 || row.fake_mine_mask != 0)) {
    cost += MISSED_FOOTSWITCH;
  }
  return cost;
}

float StepParityCost::calcFacingCosts(const State* resultState) {
  int endLeftHeel = resultState->whereTheFeetAre[LEFT_HEEL];
  int endLeftToe = resultState->whereTheFeetAre[LEFT_TOE];
  int endRightHeel = resultState->whereTheFeetAre[RIGHT_HEEL];
  int endRightToe = resultState->whereTheFeetAre[RIGHT_TOE];

  if (endLeftToe == INVALID_COLUMN) {
    endLeftToe = endLeftHeel;
  }
  if (endRightToe == INVALID_COLUMN) {
    endRightToe = endRightHeel;
  }

  float heelFacingPenalty =
      layout->getXFacingPenalty(endLeftHeel, endRightHeel) * FACING;
  float toesFacingPenalty =
      layout->getXFacingPenalty(endLeftToe, endRightToe) * FACING;
  float leftFacingPenalty =
      layout->getYFacingPenalty(endLeftHeel, endLeftToe) * FACING;
  float rightFacingPenalty =
      layout->getYFacingPenalty(endRightHeel, endRightToe) * FACING;

  float cost = heelFacingPenalty + toesFacingPenalty + leftFacingPenalty +
               rightFacingPenalty;
  return cost;
}

float StepParityCost::calcSpinCosts(
    const State* initialState, const State* resultState) {
  float cost = 0;

  int endLeftHeel = resultState->whereTheFeetAre[LEFT_HEEL];
  int endLeftToe = resultState->whereTheFeetAre[LEFT_TOE];
  int endRightHeel = resultState->whereTheFeetAre[RIGHT_HEEL];
  int endRightToe = resultState->whereTheFeetAre[RIGHT_TOE];

  if (endLeftToe == INVALID_COLUMN) {
    endLeftToe = endLeftHeel;
  }
  if (endRightToe == INVALID_COLUMN) {
    endRightToe = endRightHeel;
  }

  // spin
  StagePoint previousLeftPos = layout->averagePoint(
      initialState->whereTheFeetAre[LEFT_HEEL],
      initialState->whereTheFeetAre[LEFT_TOE]);
  StagePoint previousRightPos = layout->averagePoint(
      initialState->whereTheFeetAre[RIGHT_HEEL],
      initialState->whereTheFeetAre[RIGHT_TOE]);
  StagePoint leftPos = layout->averagePoint(endLeftHeel, endLeftToe);
  StagePoint rightPos = layout->averagePoint(endRightHeel, endRightToe);

  if (rightPos.x < leftPos.x && previousRightPos.x < previousLeftPos.x &&
      rightPos.y < leftPos.y && previousRightPos.y > previousLeftPos.y) {
    cost += SPIN;
  }
  if (rightPos.x < leftPos.x && previousRightPos.x < previousLeftPos.x &&
      rightPos.y > leftPos.y && previousRightPos.y < previousLeftPos.y) {
    cost += SPIN;
  }
  return cost;
}

// Footswitches are harder to do when they get too slow.
// Notes with an elapsed time greater than this will incur a penalty
float StepParityCost::calcFootswitchCost(
    const State* initialState, const FootPlacement& columns, const Row& row,
    float elapsedTime, int columnCount) {
  if (elapsedTime < SLOW_FOOTSWITCH_THRESHOLD ||
      elapsedTime >= SLOW_FOOTSWITCH_IGNORE) {
    return 0.0f;
  }

  // footswitching has no penalty if there's a mine nearby
  if (row.mine_mask != 0 || row.fake_mine_mask != 0) {
    return 0.0f;
  }

  float cost = 0;
  float timeScaled = elapsedTime - SLOW_FOOTSWITCH_THRESHOLD;

  for (int i = 0; i < columnCount; i++) {
    if (initialState->combinedColumns[i] == NONE || columns[i] == NONE) {
      continue;
    }

    if (initialState->combinedColumns[i] != columns[i] &&
        initialState->combinedColumns[i] != OTHER_PART_OF_FOOT[columns[i]]) {
      cost +=
          (timeScaled / (SLOW_FOOTSWITCH_THRESHOLD + timeScaled)) * FOOTSWITCH;
      break;
    }
  }
  return cost;
}

float StepParityCost::calcSideswitchCost(
    const State* initialState, const State* resultState,
    const FootPlacement& columns) {
  float cost = 0;
  for (auto c : layout->sideArrows) {
    if (initialState->combinedColumns[c] != columns[c] && columns[c] != NONE &&
        initialState->combinedColumns[c] != NONE &&
        !resultState->didTheFootMove[initialState->combinedColumns[c]]) {
      cost += SIDESWITCH;
    }
  }
  return cost;
}

// Jacks are harder to do the faster they are.
// Add a penalty when they get faster than 16ths at 150bpm (0.1 seconds)
float StepParityCost::calcJackCost(
    bool movedLeft, bool movedRight, bool jackedLeft, bool jackedRight,
    float elapsedTime) {
  float cost = 0;
  // weighting for jacking two notes too close to eachother
  if (elapsedTime < JACK_THRESHOLD && movedLeft != movedRight) {
    float timeScaled = JACK_THRESHOLD - elapsedTime;
    if (jackedLeft || jackedRight) {
      cost += (1 / timeScaled - 1 / JACK_THRESHOLD) * JACK;
    }
  }

  return cost;
}

float StepParityCost::calcBigMovementsQuicklyCost(
    const State* initialState, const State* resultState, float elapsedTime) {
  float cost = 0;
  for (StepParity::Foot foot : FEET) {
    if ((resultState->moved_mask & FOOT_MASKS[foot]) == 0) {
      continue;
    }

    int initialPosition = initialState->whereTheFeetAre[foot];
    if (initialPosition == INVALID_COLUMN) {
      continue;
    }

    int resultPosition = resultState->whatNoteTheFootIsHitting[foot];

    // If we're bracketing something, and the toes are now where the heel
    // was, then we don't need to worry about it, we're not actually moving
    // the foot very far
    bool isBracketing =
        resultState->whatNoteTheFootIsHitting[OTHER_PART_OF_FOOT[foot]] !=
        INVALID_COLUMN;
    if (isBracketing &&
        resultState->whatNoteTheFootIsHitting[OTHER_PART_OF_FOOT[foot]] ==
            initialPosition) {
      continue;
    }

    float dist =
        (layout->getDistance(initialPosition, resultPosition) * DISTANCE) /
        elapsedTime;
    // Otherwise if we're still bracketing, this is probably a less drastic
    // movement
    if (isBracketing) {
      dist = dist * 0.2;
    }
    cost += dist;
  }

  return cost;
}

bool StepParityCost::didDoubleStep(
    const State* initialState, const Row& row, const Row* previousRow,
    bool movedLeft, bool jackedLeft, bool movedRight, bool jackedRight) {
  bool doublestepped = false;
  if (movedLeft && !jackedLeft &&
      ((initialState->didTheFootMove[LEFT_HEEL] &&
        !initialState->isTheFootHolding[LEFT_HEEL]) ||
       (initialState->didTheFootMove[LEFT_TOE] &&
        !initialState->isTheFootHolding[LEFT_TOE]))) {
    doublestepped = true;
  }
  if (movedRight && !jackedRight &&
      ((initialState->didTheFootMove[RIGHT_HEEL] &&
        !initialState->isTheFootHolding[RIGHT_HEEL]) ||
       (initialState->didTheFootMove[RIGHT_TOE] &&
        !initialState->isTheFootHolding[RIGHT_TOE]))) {
    doublestepped = true;
  }

  if (previousRow != nullptr) {
    const StepParity::Row& lastRow = *previousRow;
    for (StepParity::IntermediateNoteData hold : lastRow.holds) {
      if (hold.type == TapNoteType_Empty) {
        continue;
      }
      float endBeat = row.beat;
      float startBeat = lastRow.beat;
      // if a hold tail extends past the last row & ends in between, we can
      // doublestep
      if (hold.beat + hold.hold_length > startBeat &&
          hold.beat + hold.hold_length < endBeat) {
        doublestepped = false;
      }
      // if the hold tail extends past this row, we can doublestep
      if (hold.beat + hold.hold_length >= endBeat) {
        doublestepped = false;
      }
    }
  }
  return doublestepped;
}

bool StepParityCost::didJackLeft(
    const State* initialState, const State* resultState, int leftHeel,
    int leftToe, bool movedLeft, bool didJump) {
  bool jackedLeft = false;
  if (!didJump && movedLeft) {
    if (leftHeel > INVALID_COLUMN &&
        initialState->combinedColumns[leftHeel] == LEFT_HEEL &&
        !resultState->isTheFootHolding[LEFT_HEEL] &&
        ((initialState->didTheFootMove[LEFT_HEEL] &&
          !initialState->isTheFootHolding[LEFT_HEEL]) ||
         (initialState->didTheFootMove[LEFT_TOE] &&
          !initialState->isTheFootHolding[LEFT_TOE]))) {
      jackedLeft = true;
    }
    if (leftToe > INVALID_COLUMN &&
        initialState->combinedColumns[leftToe] == LEFT_TOE &&
        !resultState->isTheFootHolding[LEFT_TOE] &&
        ((initialState->didTheFootMove[LEFT_HEEL] &&
          !initialState->isTheFootHolding[LEFT_HEEL]) ||
         (initialState->didTheFootMove[LEFT_TOE] &&
          !initialState->isTheFootHolding[LEFT_TOE]))) {
      jackedLeft = true;
    }
  }
  return jackedLeft;
}

bool StepParityCost::didJackRight(
    const State* initialState, const State* resultState, int rightHeel,
    int rightToe, bool movedRight, bool didJump) {
  bool jackedRight = false;
  if (!didJump && movedRight) {
    if (rightHeel > INVALID_COLUMN &&
        initialState->combinedColumns[rightHeel] == RIGHT_HEEL &&
        !resultState->isTheFootHolding[RIGHT_HEEL] &&
        ((initialState->didTheFootMove[RIGHT_HEEL] &&
          !initialState->isTheFootHolding[RIGHT_HEEL]) ||
         (initialState->didTheFootMove[RIGHT_TOE] &&
          !initialState->isTheFootHolding[RIGHT_TOE]))) {
      jackedRight = true;
    }
    if (rightToe > INVALID_COLUMN &&
        initialState->combinedColumns[rightToe] == RIGHT_TOE &&
        !resultState->isTheFootHolding[RIGHT_TOE] &&
        ((initialState->didTheFootMove[RIGHT_HEEL] &&
          !initialState->isTheFootHolding[RIGHT_HEEL]) ||
         (initialState->didTheFootMove[RIGHT_TOE] &&
          !initialState->isTheFootHolding[RIGHT_TOE]))) {
      jackedRight = true;
    }
  }
  return jackedRight;
}
