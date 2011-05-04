// -------------------------------------------------------------------------
// File::   plugin_L1GctHFBitCounts.cc
// Created: Tue May  3 03:49:10 2011 by mkplugins.py
// $Revision: 1.2 $
// -------------------------------------------------------------------------
#include "PhysicsTools/TheNtupleMaker/interface/Buffer.h"
#include "PhysicsTools/TheNtupleMaker/interface/pluginfactory.h"
#include "DataFormats/L1GlobalCaloTrigger/interface/L1GctHFBitCounts.h"

typedef Buffer<L1GctHFBitCounts, false> L1GctHFBitCounts_t;
DEFINE_EDM_PLUGIN(BufferFactory, L1GctHFBitCounts_t,
                  "L1GctHFBitCounts");
