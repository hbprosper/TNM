// -------------------------------------------------------------------------
// File::   plugin_recoCaloTau.cc
// Created: Tue May  3 03:49:10 2011 by mkplugins.py
// $Revision: 1.2 $
// -------------------------------------------------------------------------
#include "PhysicsTools/TheNtupleMaker/interface/Buffer.h"
#include "PhysicsTools/TheNtupleMaker/interface/pluginfactory.h"
#include "DataFormats/TauReco/interface/CaloTau.h"

typedef Buffer<reco::CaloTau, false> recoCaloTau_t;
DEFINE_EDM_PLUGIN(BufferFactory, recoCaloTau_t,
                  "recoCaloTau");
