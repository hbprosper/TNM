// -------------------------------------------------------------------------
// File::   plugin_recoSoftLeptonTagInfo.cc
// Created: Tue May  3 03:49:10 2011 by mkplugins.py
// $Revision: 1.2 $
// -------------------------------------------------------------------------
#include "PhysicsTools/TheNtupleMaker/interface/Buffer.h"
#include "PhysicsTools/TheNtupleMaker/interface/pluginfactory.h"
#include "DataFormats/BTauReco/interface/SoftLeptonTagInfo.h"

typedef Buffer<reco::SoftLeptonTagInfo, false> recoSoftLeptonTagInfo_t;
DEFINE_EDM_PLUGIN(BufferFactory, recoSoftLeptonTagInfo_t,
                  "recoSoftLeptonTagInfo");
