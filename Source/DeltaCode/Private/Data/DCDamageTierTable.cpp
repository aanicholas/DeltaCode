/*
 * DeltaCode - Unreal Engine Code Helper
 * Copyright (c) 2026 Andrew Nicholas
 *
 * License: GPL v3 or later. See repository LICENSE.
 */

#include "Data/DCDamageTierTable.h"

// FDCDamageConfig's own constructor seeds the default tier magnitudes and
// the Data.Damage SetByCaller tag, so this UPrimaryDataAsset is fully
// usable without an explicit ctor here.
