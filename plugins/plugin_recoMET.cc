// -------------------------------------------------------------------------
// File::   plugin_recoMET.cc
// Created: Tue May  3 03:49:10 2011 by mkplugins.py
// $Revision: 1.2 $
// -------------------------------------------------------------------------
#include "PhysicsTools/TheNtupleMaker/interface/Buffer.h"
#include "PhysicsTools/TheNtupleMaker/interface/pluginfactory.h"
#include "DataFormats/METReco/interface/MET.h"

typedef Buffer<reco::MET, false> recoMET_t;
DEFINE_EDM_PLUGIN(BufferFactory, recoMET_t,
                  "recoMET");
