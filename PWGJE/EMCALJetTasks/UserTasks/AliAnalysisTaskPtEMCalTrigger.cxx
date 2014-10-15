/**************************************************************************
 * Copyright(c) 1998-2007, ALICE Experiment at CERN, All rights reserved. *
 *                                                                        *
 * Author: The ALICE Off-line Project.                                    *
 * Contributors are mentioned in the code where appropriate.              *
 *                                                                        *
 * Permission to use, copy, modify and distribute this software and its   *
 * documentation strictly for non-commercial purposes is hereby granted   *
 * without fee, provided that the above copyright notice appears in all   *
 * copies and that both the copyright notice and this permission notice   *
 * appear in the supporting documentation. The authors make no claims     *
 * about the suitability of this software for any purpose. It is          *
 * provided "as is" without express or implied warranty.                  *
 **************************************************************************/

/*
 * Analysis task of the pt analysis on EMCal-triggered events
 *
 *   Author: Markus Fasel
 */

#include <map>
#include <cstring>
#include <iostream>
#include <memory>
#include <vector>
#include <string>
#include <sstream>

#include <TClonesArray.h>
#include <TDirectory.h>
#include <TH1.h>
#include <THashList.h>
#include <TList.h>
#include <TKey.h>
#include <TMath.h>
#include <TObjArray.h>
#include <TString.h>

#include "AliAODEvent.h"
#include "AliESDEvent.h"
#include "AliInputEventHandler.h"
#include "AliMCEvent.h"
#include "AliParticleContainer.h"
#include "AliVCluster.h"
#include "AliVParticle.h"
#include "AliVTrack.h"
#include "AliVVertex.h"

#include "AliEmcalTriggerPatchInfo.h"
#include "AliEMCalHistoContainer.h"
#include "AliEMCalPtTaskVTrackSelection.h"
#include "AliEMCalPtTaskTrackSelectionAOD.h"
#include "AliEMCalPtTaskTrackSelectionESD.h"
#include "AliAnalysisTaskPtEMCalTrigger.h"

ClassImp(EMCalTriggerPtAnalysis::AliAnalysisTaskPtEMCalTrigger)

namespace EMCalTriggerPtAnalysis {

  //______________________________________________________________________________
  AliAnalysisTaskPtEMCalTrigger::AliAnalysisTaskPtEMCalTrigger():
                		    AliAnalysisTaskEmcal(),
                		    fHistos(NULL),
                		    fListTrackCuts(NULL),
                		    fEtaRange(),
                		    fPtRange(),
                		    fSwapEta(kFALSE),
                		    fUseTriggersFromTriggerMaker(kFALSE)
  {
    /*
     * Dummy constructor, initialising the values with default (NULL) values
     */
  }

  //______________________________________________________________________________
  AliAnalysisTaskPtEMCalTrigger::AliAnalysisTaskPtEMCalTrigger(const char *name):
                		    AliAnalysisTaskEmcal(name, kTRUE),
                		    fHistos(NULL),
                		    fListTrackCuts(NULL),
                		    fEtaRange(),
                		    fPtRange(),
                		    fSwapEta(kFALSE),
                		    fUseTriggersFromTriggerMaker(kFALSE)
  {
    /*
     * Main constructor, setting default values for eta and zvertex cut
     */

    fListTrackCuts = new TList;
    fListTrackCuts->SetOwner(false);

    // Set default cuts
    fEtaRange.SetLimits(-0.8, 0.8);
    fPtRange.SetLimits(0.15, 100.);
    SetMakeGeneralHistograms(kTRUE);
  }

  //______________________________________________________________________________
  AliAnalysisTaskPtEMCalTrigger::~AliAnalysisTaskPtEMCalTrigger(){
    /*
     * Destructor, deleting output
     */
    //if(fTrackSelection) delete fTrackSelection;
    if(fHistos) delete fHistos;
    if(fListTrackCuts) delete fListTrackCuts;
  }

  //______________________________________________________________________________
  void AliAnalysisTaskPtEMCalTrigger::UserCreateOutputObjects(){
    /*
     * Create the list of output objects and define the histograms.
     * Also adding the track cuts to the list of histograms.
     */
    AliAnalysisTaskEmcal::UserCreateOutputObjects();
    TString trackContainerName = "ESDFilterTracks", clusterContainerName = "EmcCaloClusters";
    if(!fIsEsd){
      trackContainerName = "AODFilterTracks";
      clusterContainerName = "EmcCaloClusters";
    }
    AliParticleContainer *trackContainer = this->AddParticleContainer(trackContainerName.Data());
    trackContainer->SetClassName("AliVTrack");
    this->AddClusterContainer(clusterContainerName.Data());
    this->SetCaloTriggerPatchInfoName("EmcalTriggers");
    fHistos = new AliEMCalHistoContainer("PtEMCalTriggerHistograms");
    fHistos->ReleaseOwner();

    std::map<std::string, std::string> triggerCombinations;
    const char *triggernames[12] = {"MinBias", "EMCJHigh", "EMCJLow", "EMCGHigh", "EMCGLow", "NoEMCal", "EMCHighBoth", "EMCHighGammaOnly", "EMCHighJetOnly", "EMCLowBoth", "EMCLowGammaOnly", "EMCLowJetOnly"},
        *bitnames[4] = {"CINT7", "EMC7", "kEMCEGA", "kEMCEJE"};
    // Define axes for the trigger correlation histogram
    const TAxis *triggeraxis[5]; memset(triggeraxis, 0, sizeof(const TAxis *) * 5);
    const TAxis *bitaxes[4]; memset(bitaxes, 0, sizeof(TAxis *) * 4);
    const char *binlabels[2] = {"OFF", "ON"};
    TAxis mytrgaxis[5], mybitaxis[4];
    for(int itrg = 0; itrg < 5; ++itrg){
      DefineAxis(mytrgaxis[itrg], triggernames[itrg], triggernames[itrg], 2, -0.5, 1.5, binlabels);
      triggeraxis[itrg] = mytrgaxis+itrg;
      if(itrg < 4){
        DefineAxis(mybitaxis[itrg], bitnames[itrg], bitnames[itrg], 2, -0.5, 1.5, binlabels);
        bitaxes[itrg] = mybitaxis+itrg;
      }
    }
    // Define names and titles for different triggers in the histogram container
    triggerCombinations.insert(std::pair<std::string,std::string>(triggernames[0], "min. bias events"));
    triggerCombinations.insert(std::pair<std::string,std::string>(triggernames[1], "jet-triggered events (high threshold)"));
    triggerCombinations.insert(std::pair<std::string,std::string>(triggernames[2], "jet-triggered events (low threshold)"));
    triggerCombinations.insert(std::pair<std::string,std::string>(triggernames[3], "gamma-triggered events (high threshold)"));
    triggerCombinations.insert(std::pair<std::string,std::string>(triggernames[4], "gamma-triggered events (low threshold)"));
    triggerCombinations.insert(std::pair<std::string,std::string>(triggernames[5], "non-EMCal-triggered events"));
    triggerCombinations.insert(std::pair<std::string,std::string>(triggernames[6], "jet and gamma triggered events (high threshold)"));
    triggerCombinations.insert(std::pair<std::string,std::string>(triggernames[7], "exclusively gamma-triggered events (high threshold)"));
    triggerCombinations.insert(std::pair<std::string,std::string>(triggernames[8], "exclusively jet-triggered events (high threshold)"));
    triggerCombinations.insert(std::pair<std::string,std::string>(triggernames[9], "jet and gamma triggered events (low threshold)"));
    triggerCombinations.insert(std::pair<std::string,std::string>(triggernames[10], "exclusively gamma-triggered events (low threshold)"));
    triggerCombinations.insert(std::pair<std::string,std::string>(triggernames[11], "exclusively-triggered events (low threshold)"));
    // Define axes for the pt histogram
    // Dimensions:
    // 1. pt
    // 2. eta
    // 3. phi
    // 4. vertex
    // 5. pileup (0 = all events, 1 = after pileup rejection)
    // 6. track cuts (0 = no cuts; 1 = after std cuts)
    TArrayD ptbinning, zvertexBinning, etabinning, pileupaxis(3);
    pileupaxis[0] = -0.5; pileupaxis[1] = 0.5; pileupaxis[2] = 1.5;
    CreateDefaultPtBinning(ptbinning);
    CreateDefaultZVertexBinning(zvertexBinning);
    CreateDefaultEtaBinning(etabinning);
    TAxis htrackaxes[7];
    DefineAxis(htrackaxes[0], "pt", "p_{t} (GeV/c)", ptbinning);
    DefineAxis(htrackaxes[1], "eta", "#eta", etabinning);
    DefineAxis(htrackaxes[2], "phi", "#phi", 20, 0, 2 * TMath::Pi());
    DefineAxis(htrackaxes[3], "zvertex", "z_{V} (cm)", zvertexBinning);
    DefineAxis(htrackaxes[4], "pileup", "Pileup rejection", 2, -0.5, 1.5);
    DefineAxis(htrackaxes[5], "trackcuts", "Track Cuts", (fListTrackCuts ? fListTrackCuts->GetEntries() : 0) + 1, -0.5, (fListTrackCuts ? fListTrackCuts->GetEntries() : 0) + 0.5);
    DefineAxis(htrackaxes[6], "mbtrigger", "Has MB trigger", 2, -0.5, 1.5);
    const TAxis *trackaxes[7];
    for(int iaxis = 0; iaxis < 7; ++iaxis) trackaxes[iaxis] = htrackaxes + iaxis;
    TAxis hclusteraxes[4];
    DefineAxis(hclusteraxes[0], "energy", "E (GeV)", ptbinning);
    DefineAxis(hclusteraxes[1], "zvertex", "z_{V} (cm)", zvertexBinning);
    DefineAxis(hclusteraxes[2], "pileup", "Pileup rejection", 2,1 -0.5, 1.5);
    DefineAxis(hclusteraxes[3], "mbtrigger", "Has MB trigger", 2, -0.5, 1.5);
    const TAxis *clusteraxes[4];
    for(int iaxis = 0; iaxis < 4; ++iaxis) clusteraxes[iaxis] = hclusteraxes + iaxis;
    TAxis hpatchenergyaxes[4];
    DefineAxis(hpatchenergyaxes[0], "energy", "Patch energy (GeV)", 100, 0., 100.);
    DefineAxis(hpatchenergyaxes[1], "eta", "#eta", etabinning);
    DefineAxis(hpatchenergyaxes[2], "phi", "#phi",  20, 0, 2 * TMath::Pi());
    DefineAxis(hpatchenergyaxes[3], "isMain", "Main trigger", 2, -0.5, 1.5);
    const TAxis *patchenergyaxes[4];
    for(int iaxis = 0; iaxis < 4; ++iaxis) patchenergyaxes[iaxis] = hpatchenergyaxes + iaxis;
    TAxis hpatchampaxes[4];
    DefineAxis(hpatchampaxes[0], "amplitude", "Patch energy (GeV)", 2000, 0., 2000.);
    DefineAxis(hpatchampaxes[1], "eta", "#eta", etabinning);
    DefineAxis(hpatchampaxes[2], "phi", "#phi",  20, 0, 2 * TMath::Pi());
    DefineAxis(hpatchampaxes[3], "isMain", "Main trigger", 2, -0.5, 1.5);
    const TAxis *patchampaxes[4];
    for(int iaxis = 0; iaxis < 4; ++iaxis) patchampaxes[iaxis] = hpatchampaxes + iaxis;
    try{
      std::string patchnames[] = {"Level0", "JetHigh", "JetLow", "GammaHigh", "GammaLow"};
      for(std::string * triggerpatch = patchnames; triggerpatch < patchnames + sizeof(patchnames)/sizeof(std::string); ++triggerpatch){
        fHistos->CreateTHnSparse(Form("Energy%s", triggerpatch->c_str()), Form("Patch energy for %s trigger patches", triggerpatch->c_str()), 4, patchenergyaxes);
        fHistos->CreateTHnSparse(Form("EnergyRough%s", triggerpatch->c_str()), Form("Rough patch energy for %s trigger patches", triggerpatch->c_str()), 4, patchenergyaxes);
        fHistos->CreateTHnSparse(Form("Amplitude%s", triggerpatch->c_str()), Form("Patch amplitude for %s trigger patches", triggerpatch->c_str()), 4, patchampaxes);
      }

      // Create histogram for MC-truth
      fHistos->CreateTHnSparse("hMCtrueParticles", "Particle-based histogram for MC-true particles", 5, trackaxes);
      for(std::map<std::string,std::string>::iterator it = triggerCombinations.begin(); it != triggerCombinations.end(); ++it){
        const std::string name = it->first, &title = it->second;
        // Create event-based histogram
        fHistos->CreateTH2(Form("hEventHist%s", name.c_str()), Form("Event-based data for %s events; pileup rejection; z_{V} (cm)", title.c_str()), pileupaxis, zvertexBinning);
        // Create track-based histogram
        fHistos->CreateTHnSparse(Form("hTrackHist%s", name.c_str()), Form("Track-based data for %s events", title.c_str()), 7, trackaxes);
        fHistos->CreateTHnSparse(Form("hTrackInAcceptanceHist%s", name.c_str()), Form("Track-based data for %s events", title.c_str()), 7, trackaxes);
        fHistos->CreateTHnSparse(Form("hMCTrackHist%s", name.c_str()), Form("Track-based data for %s events with MC kinematics", title.c_str()), 7, trackaxes);
        fHistos->CreateTHnSparse(Form("hMCTrackInAcceptanceHist%s", name.c_str()), Form("Track-based data for %s events with MC kinematics", title.c_str()), 7, trackaxes);
        // Create cluster-based histogram (Uncalibrated and calibrated clusters)
        fHistos->CreateTHnSparse(Form("hClusterCalibHist%s", name.c_str()), Form("Calib. cluster-based histogram for %s events", title.c_str()), 4, clusteraxes);
        fHistos->CreateTHnSparse(Form("hClusterUncalibHist%s", name.c_str()), Form("Uncalib. cluster-based histogram for %s events", title.c_str()), 4, clusteraxes);
      }
      fHistos->CreateTHnSparse("hEventTriggers", "Trigger type per event", 5, triggeraxis);
      fHistos->CreateTHnSparse("hEventsTriggerbit", "Trigger bits for the different events", 4, bitaxes);
    } catch (HistoContainerContentException &e){
      std::stringstream errormessage;
      errormessage << "Creation of histogram failed: " << e.what();
      AliError(errormessage.str().c_str());
    }
    fOutput->Add(fHistos->GetListOfHistograms());
    if(fListTrackCuts && fListTrackCuts->GetEntries()){
      TIter cutIter(fListTrackCuts);
      AliEMCalPtTaskVTrackSelection *cutObject(NULL);
      while((cutObject = dynamic_cast<AliEMCalPtTaskVTrackSelection *>(cutIter()))){
        AliESDtrackCuts *cuts = dynamic_cast<AliESDtrackCuts *>(cutObject->GetTrackCuts());
        if(cuts){
          cuts->DefineHistograms();
          fOutput->Add(cuts);
        }
      }
    }
    PostData(1, fOutput);
  }

  //______________________________________________________________________________
  Bool_t AliAnalysisTaskPtEMCalTrigger::Run(){
    /*
     * Runs the event loop
     *
     * @param option: Additional options
     */
    // Common checks: Have SPD vertex and primary vertex from tracks, and both need to have at least one contributor
    AliDebug(1,Form("Number of calibrated clusters: %d", fCaloClusters->GetEntries()));
    AliDebug(1,Form("Number of matched tracks: %d", fTracks->GetEntries()));
    if(fMCEvent){
      // Build always trigger strig from the trigger maker in case of MC
      fUseTriggersFromTriggerMaker = kTRUE;
    }

    // Loop over trigger patches, fill patch energy
    AliEmcalTriggerPatchInfo *triggerpatch(NULL);
    TIter patchIter(this->fTriggerPatchInfo);
    while((triggerpatch = dynamic_cast<AliEmcalTriggerPatchInfo *>(patchIter()))){
      double triggerpatchinfo[4] = {triggerpatch->GetPatchE(), triggerpatch->GetEtaGeo(), triggerpatch->GetPhiGeo(), triggerpatch->IsMainTrigger() ? 1 : 0};
      double triggerpatchinfoamp[4] = {triggerpatch->GetADCAmp(), triggerpatch->GetEtaGeo(), triggerpatch->GetPhiGeo(), triggerpatch->IsMainTrigger() ? 1 : 0};
      double triggerpatchinfoer[4] = {triggerpatch->GetADCAmpGeVRough(), triggerpatch->GetEtaGeo(), triggerpatch->GetPhiGeo(), triggerpatch->IsMainTrigger() ? 1 : 0};
      if(triggerpatch->IsJetHigh()){
        fHistos->FillTHnSparse("EnergyJetHigh", triggerpatchinfo);
        fHistos->FillTHnSparse("AmplitudeJetHigh", triggerpatchinfoamp);
        fHistos->FillTHnSparse("EnergyRoughJetHigh", triggerpatchinfoer);
      }
      if(triggerpatch->IsJetLow()){
        fHistos->FillTHnSparse("EnergyJetLow", triggerpatchinfo);
        fHistos->FillTHnSparse("AmplitudeJetLow", triggerpatchinfoamp);
        fHistos->FillTHnSparse("EnergyRoughJetLow", triggerpatchinfoer);
      }
      if(triggerpatch->IsGammaHigh()){
        fHistos->FillTHnSparse("EnergyGammaHigh", triggerpatchinfo);
        fHistos->FillTHnSparse("AmplitudeGammaHigh", triggerpatchinfoamp);
        fHistos->FillTHnSparse("EnergyRoughGammaHigh", triggerpatchinfoer);
      }
      if(triggerpatch->IsGammaLow()){
        fHistos->FillTHnSparse("EnergyGammaLow", triggerpatchinfo);
        fHistos->FillTHnSparse("AmplitudeGammaLow", triggerpatchinfoamp);
        fHistos->FillTHnSparse("EnergyRoughGammaLow", triggerpatchinfoer);
      }
      if(triggerpatch->IsLevel0()){
        fHistos->FillTHnSparse("EnergyLevel0", triggerpatchinfo);
        fHistos->FillTHnSparse("AmplitudeLevel0", triggerpatchinfoamp);
        fHistos->FillTHnSparse("EnergyRoughLevel0", triggerpatchinfoer);
      }
    }

    const AliVVertex *vtxTracks = fInputEvent->GetPrimaryVertex(),
        *vtxSPD = GetSPDVertex();
    if(!(vtxTracks && vtxSPD)) return false;
    if(vtxTracks->GetNContributors() < 1 || vtxSPD->GetNContributors() < 1) return false;

    double triggers[5]; memset(triggers, 0, sizeof(double) * 5);
    double triggerbits[4]; memset(triggerbits, 0, sizeof(double) * 4);
    if(fInputHandler->IsEventSelected() & AliVEvent::kINT7){
      triggers[0] = 1.;
      triggerbits[0] = 1.;
    }

    // check triggerbits
    if(fInputHandler->IsEventSelected() & AliVEvent::kEMC7){
      triggerbits[1] = 1.;
    }
    if(fInputHandler->IsEventSelected() & AliVEvent::kEMCEGA){
      triggerbits[2] = 1.;
    }
    if(fInputHandler->IsEventSelected() & AliVEvent::kEMCEJE){
      triggerbits[3] = 1.;
    }
    try{
      fHistos->FillTHnSparse("hEventsTriggerbit", triggerbits);
    } catch(HistoContainerContentException &e) {
      std::stringstream errormessage;
      errormessage << "Filling of histogram failed: " << e.what();
      AliError(errormessage.str().c_str());
    }

    std::vector<std::string> triggerstrings;
    // EMCal-triggered event, distinguish types
    TString trgstr(fUseTriggersFromTriggerMaker ? BuildTriggerString() : fInputEvent->GetFiredTriggerClasses());
    AliDebug(1, Form("Triggerstring: %s\n", trgstr.Data()));
    if(trgstr.Contains("EJ1")){
      triggerstrings.push_back("EMCJHigh");
      triggers[1] = 1;
      if(trgstr.Contains("EG1"))
        triggerstrings.push_back("EMCHighBoth");
      else
        triggerstrings.push_back("EMCHighJetOnly");
    }
    if(trgstr.Contains("EJ2")){
      triggerstrings.push_back("EMCJLow");
      triggers[2] = 1;
      if(trgstr.Contains("EG2"))
        triggerstrings.push_back("EMCLowBoth");
      else
        triggerstrings.push_back("EMCLowJetOnly");
    }
    if(trgstr.Contains("EG1")){
      triggerstrings.push_back("EMCGHigh");
      triggers[3] = 1;
      if(!trgstr.Contains("EJ1"))
        triggerstrings.push_back("EMCHighGammaOnly");
    }
    if(trgstr.Contains("EG2")){
      triggerstrings.push_back("EMCGLow");
      triggers[4] = 1;
      if(!trgstr.Contains("EJ2"))
        triggerstrings.push_back("EMCLowGammaOnly");
    }

    try{
      fHistos->FillTHnSparse("hEventTriggers", triggers);
    } catch (HistoContainerContentException &e){
      std::stringstream errormessage;
      errormessage << "Filling of histogram failed: " << e.what();
      AliError(errormessage.str().c_str());
    }

    // apply event selection: Combine the Pileup cut from SPD with the other pA Vertex selection cuts.
    bool isPileupEvent = fInputEvent->IsPileupFromSPD(3, 0.8, 3., 2., 5.);
    isPileupEvent = isPileupEvent || (TMath::Abs(vtxTracks->GetZ() - vtxSPD->GetZ()) > 0.5);
    double covSPD[6]; vtxSPD->GetCovarianceMatrix(covSPD);
    isPileupEvent = isPileupEvent || (TString(vtxSPD->GetTitle()).Contains("vertexer:Z") && TMath::Sqrt(covSPD[5]) > 0.25);

    // Fill event-based histogram
    const double &zv = vtxTracks->GetZ();
    if(triggers[0]) FillEventHist("MinBias", zv, isPileupEvent);
    if(!triggerstrings.size()) // Non-EMCal-triggered
      FillEventHist("NoEMCal", zv, isPileupEvent);
    else{
      // EMCal-triggered events
      for(std::vector<std::string>::iterator it = triggerstrings.begin(); it != triggerstrings.end(); ++it)
        FillEventHist(it->c_str(), zv, isPileupEvent);
    }

    // Fill MC truth
    if(fMCEvent){
      for(int ipart = 0; ipart < fMCEvent->GetNumberOfTracks(); ipart++){
        // Select only physical primary particles
        AliVParticle *part = fMCEvent->GetTrack(ipart);
        if(!fEtaRange.IsInRange(part->Eta())) continue;
        if(!fPtRange.IsInRange(part->Pt())) continue;
        if(!fMCEvent->IsPhysicalPrimary(ipart)) continue;
        FillMCParticleHist(part, zv, isPileupEvent);
      }
    }

    AliVTrack *track(NULL);
    // Loop over all tracks (No cuts applied)
    TIter allTrackIter(fTracks);
    while((track = dynamic_cast<AliVTrack *>(allTrackIter()))){
      if(!IsTrueTrack(track)) continue;
      if(!fEtaRange.IsInRange(track->Eta())) continue;
      if(!fPtRange.IsInRange(track->Pt())) continue;
      if(triggers[0]) FillTrackHist("MinBias", track, zv, isPileupEvent, 0, triggers[0]);
      if(!triggerstrings.size()) // Non-EMCal-triggered
        FillTrackHist("NoEMCal", track, zv, isPileupEvent, 0, triggers[0]);
      else {
        // EMCal-triggered events
        for(std::vector<std::string>::iterator it = triggerstrings.begin(); it != triggerstrings.end(); ++it)
          FillTrackHist(it->c_str(), track, zv, isPileupEvent, 0, triggers[0]);
      }
    }

    // Now apply track selection cuts
    // allow for several track selections to be tested at the same time
    // each track selection gets a different cut ID starting from 1
    // cut ID 0 is reserved for the case of no cuts
    if(fListTrackCuts && fListTrackCuts->GetEntries()){
      for(int icut = 0; icut < fListTrackCuts->GetEntries(); icut++){
        AliEMCalPtTaskVTrackSelection *trackSelection = static_cast<AliEMCalPtTaskVTrackSelection *>(fListTrackCuts->At(icut));
        TIter trackIter(trackSelection->GetAcceptedTracks(fTracks));
        while((track = dynamic_cast<AliVTrack *>(trackIter()))){
          //if(!IsTrueTrack(track)) continue;
          if(!fEtaRange.IsInRange(track->Eta())) continue;
          if(!fPtRange.IsInRange(track->Pt())) continue;
          if(triggers[0]) FillTrackHist("MinBias", track, zv, isPileupEvent, icut + 1, triggers[0]);
          if(!triggerstrings.size()) // Non-EMCal-triggered
            FillTrackHist("NoEMCal", track, zv, isPileupEvent, icut + 1, triggers[0]);
          else {
            // EMCal-triggered events
            for(std::vector<std::string>::iterator it = triggerstrings.begin(); it != triggerstrings.end(); ++it)
              FillTrackHist(it->c_str(), track, zv, isPileupEvent, icut + 1, triggers[0]);
          }
        }
      }
    }

    // Next step we loop over the (uncalibrated) emcal clusters and fill histograms with the cluster energy
    const AliVCluster *clust(NULL);
    for(int icl = 0; icl < fInputEvent->GetNumberOfCaloClusters(); icl++){
      clust = fInputEvent->GetCaloCluster(icl);
      if(!clust->IsEMCAL()) continue;
      if(triggers[0]) FillClusterHist("MinBias", clust, false, zv, isPileupEvent, triggers[0]);
      if(!triggerstrings.size())	// Non-EMCal-triggered
        FillClusterHist("NoEMCal", clust, false, zv, isPileupEvent, triggers[0]);
      else{
        for(std::vector<std::string>::iterator it = triggerstrings.begin(); it != triggerstrings.end(); ++it){
          FillClusterHist(it->c_str(), clust, false, zv, isPileupEvent, triggers[0]);
        }
      }
    }

    if(fCaloClusters){
      TIter clustIter(fCaloClusters);
      while((clust = dynamic_cast<const AliVCluster *>(clustIter()))){
        if(!clust->IsEMCAL()) continue;
        if(triggers[0]) FillClusterHist("MinBias", clust, true, zv, isPileupEvent, triggers[0]);
        if(!triggerstrings.size())	// Non-EMCal-triggered
          FillClusterHist("NoEMCal", clust, true, zv, isPileupEvent, triggers[0]);
        else{
          for(std::vector<std::string>::iterator it = triggerstrings.begin(); it != triggerstrings.end(); ++it){
            FillClusterHist(it->c_str(), clust, true, zv, isPileupEvent, triggers[0]);
          }
        }
      }
    }

    PostData(1, fOutput);
    return true;
  }

  //______________________________________________________________________________
  void AliAnalysisTaskPtEMCalTrigger::CreateDefaultPtBinning(TArrayD &binning) const{
    /*
     * Creating the default pt binning.
     *
     * @param binning: Array where to store the results.
     */
    std::vector<double> mybinning;
    std::map<double,double> definitions;
    definitions.insert(std::pair<double,double>(2.5, 0.1));
    definitions.insert(std::pair<double,double>(7., 0.25));
    definitions.insert(std::pair<double,double>(15., 0.5));
    definitions.insert(std::pair<double,double>(25., 1.));
    definitions.insert(std::pair<double,double>(40., 2.5));
    definitions.insert(std::pair<double,double>(60., 5.));
    definitions.insert(std::pair<double,double>(100., 5.));
    double currentval = 0;
    for(std::map<double,double>::iterator id = definitions.begin(); id != definitions.end(); ++id){
      double limit = id->first, binwidth = id->second;
      while(currentval <= limit){
        currentval += binwidth;
        mybinning.push_back(currentval);
      }
    }
    binning.Set(mybinning.size());
    int ib = 0;
    for(std::vector<double>::iterator it = mybinning.begin(); it != mybinning.end(); ++it)
      binning[ib++] = *it;
  }

  //______________________________________________________________________________
  void AliAnalysisTaskPtEMCalTrigger::CreateDefaultZVertexBinning(TArrayD &binning) const {
    /*
     * Creating default z-Vertex binning.
     *
     * @param binning: Array where to store the results.
     */
    std::vector<double> mybinning;
    double currentval = -40;
    mybinning.push_back(currentval);
    while(currentval <= 40.){
      currentval += 1.;
      mybinning.push_back(currentval);
    }
    binning.Set(mybinning.size());
    int ib = 0;
    for(std::vector<double>::iterator it = mybinning.begin(); it != mybinning.end(); ++it)
      binning[ib++] = *it;
  }

  //______________________________________________________________________________
  void AliAnalysisTaskPtEMCalTrigger::CreateDefaultEtaBinning(TArrayD& binning) const {
    /*
     * Creating default z-Vertex binning.
     *
     * @param binning: Array where to store the results.
     */
    std::vector<double> mybinning;
    double currentval = -0.8;
    mybinning.push_back(currentval);
    while(currentval <= 0.8){
      currentval += 0.1;
      mybinning.push_back(currentval);
    }
    binning.Set(mybinning.size());
    int ib = 0;
    for(std::vector<double>::iterator it = mybinning.begin(); it != mybinning.end(); ++it)
      binning[ib++] = *it;
  }

  //______________________________________________________________________________
  void AliAnalysisTaskPtEMCalTrigger::DefineAxis(TAxis& axis, const char* name,
      const char* title, const TArrayD& binning, const char** labels) {
    /*
     * Define an axis with a given binning
     *
     * @param axis: Axis to be defined
     * @param name: Name of the axis
     * @param title: Title of the axis
     * @param binning: axis binning
     * @param labels (@optional): array of bin labels
     */
    axis.Set(binning.GetSize()-1, binning.GetArray());
    axis.SetName(name);
    axis.SetTitle(title);
    if(labels){
      for(int ib = 1; ib <= axis.GetNbins(); ++ib)
        axis.SetBinLabel(ib, labels[ib-1]);
    }
  }

  //______________________________________________________________________________
  void AliAnalysisTaskPtEMCalTrigger::DefineAxis(TAxis& axis, const char* name,
      const char* title, int nbins, double min, double max,
      const char** labels) {
    /*
     * Define an axis with number of bins from min to max
     *
     * @param axis: Axis to be defined
     * @param name: Name of the axis
     * @param title: Title of the axis
     * @param nbins: Number of bins
     * @param min: lower limit of the axis
     * @param max: upper limit of the axis
     * @param labels (@optional): array of bin labels
     */
    axis.Set(nbins, min, max);
    axis.SetName(name);
    axis.SetTitle(title);
    if(labels){
      for(int ib = 1; ib <= axis.GetNbins(); ++ib)
        axis.SetBinLabel(ib, labels[ib-1]);
    }
  }


  //______________________________________________________________________________
  void AliAnalysisTaskPtEMCalTrigger::FillEventHist(const char* trigger,
      double vz, bool isPileup) {
    /*
     * Fill event-based histogram
     *
     * @param trigger: name of the trigger configuration to be processed
     * @param vz: z-position of the vertex
     * @param isPileup: signalises if the event is flagged as pileup event
     */
    char histname[1024];
    sprintf(histname, "hEventHist%s", trigger);
    try{
      fHistos->FillTH2(histname, 0., vz);
    } catch (HistoContainerContentException &e){
      std::stringstream errormessage;
      errormessage << "Filling of histogram failed: " << e.what();
      AliError(errormessage.str().c_str());
    }
    if(!isPileup){
      try{
        fHistos->FillTH2(histname, 1., vz);
      } catch(HistoContainerContentException &e){
        std::stringstream errormessage;
        errormessage << "Filling of histogram failed: " << e.what();
        AliError(errormessage.str().c_str());
      }
    }
  }

  //______________________________________________________________________________
  void AliAnalysisTaskPtEMCalTrigger::FillTrackHist(const char* trigger,
      const AliVTrack* track, double vz, bool isPileup, int cut, bool isMinBias) {
    /*
     * Fill track-based histogram with corresponding information
     *
     * @param trigger: name of the trigger
     * @param track: ESD track selected
     * @param vz: z-position of the vertex
     * @param isPileup: flag event as pileup event
     * @param cut: id of the cut (0 = no cut)
     */
    double etasign = fSwapEta ? -1. : 1.;
    double data[7] = {TMath::Abs(track->Pt()), etasign * track->Eta(), track->Phi(), vz, 0, static_cast<double>(cut), isMinBias ? 1. : 0.};
    double dataMC[7] = {0., 0., 0., vz, 0, static_cast<double>(cut), isMinBias ? 1. : 0.};
    AliVParticle *assocMC(NULL);
    if(fMCEvent && (assocMC = fMCEvent->GetTrack(TMath::Abs(track->GetLabel())))){
      dataMC[0] = TMath::Abs(assocMC->Pt());
      dataMC[1] = etasign * assocMC->Eta();
      dataMC[2] = assocMC->Phi();
    }
    char histname[1024], histnameAcc[1024], histnameMC[1024], histnameMCAcc[1024];
    sprintf(histname, "hTrackHist%s", trigger);
    sprintf(histnameAcc, "hTrackInAcceptanceHist%s", trigger);
    sprintf(histnameMC, "hMCTrackHist%s", trigger);
    sprintf(histnameMCAcc, "hMCTrackInAcceptanceHist%s", trigger);
    Bool_t isEMCAL = kFALSE;
    if(track->IsEMCAL()){
      // Check if the cluster is matched to only one track
      AliVCluster *emcclust(NULL);
      AliDebug(2, Form("cluster id: %d\n", track->GetEMCALcluster()));
      if(fCaloClusters) {
        AliDebug(2, "Using calibrated clusters");
        emcclust = dynamic_cast<AliVCluster *>(fCaloClusters->At(track->GetEMCALcluster()));
      } else {
        AliDebug(2, "Using uncalibrated clusters");
        emcclust = fInputEvent->GetCaloCluster(track->GetEMCALcluster());
      }
      if(!emcclust) AliError("Null pointer to EMCal cluster");
      if(emcclust && emcclust->GetNTracksMatched() <= 1){
        isEMCAL = kTRUE;
      }
    }
    try{
      fHistos->FillTHnSparse(histname, data);
      if(fMCEvent) fHistos->FillTHnSparse(histnameMC, dataMC);
      if(isEMCAL){
        fHistos->FillTHnSparse(histnameAcc, data);
        if(fMCEvent) fHistos->FillTHnSparse(histnameMCAcc, dataMC);
      }
    } catch (HistoContainerContentException &e){
      std::stringstream errormessage;
      errormessage << "Filling of histogram failed: " << e.what();
      AliError(errormessage.str().c_str());
    }
    if(!isPileup){
      data[4] = 1;
      dataMC[4] = 1;
      try{
        fHistos->FillTHnSparse(histname, data);
        if(fMCEvent) fHistos->FillTHnSparse(histnameMC, dataMC);
        if(isEMCAL){
          fHistos->FillTHnSparse(histnameAcc, data);
          if(fMCEvent) fHistos->FillTHnSparse(histnameMCAcc, dataMC);
        }
      } catch (HistoContainerContentException &e){
        std::stringstream errormessage;
        errormessage << "Filling of histogram failed: " << e.what();
        AliError(errormessage.str().c_str());
      }
    }
  }

  //______________________________________________________________________________
  void AliAnalysisTaskPtEMCalTrigger::FillClusterHist(const char* trigger,
      const AliVCluster* clust, bool isCalibrated, double vz, bool isPileup, bool isMinBias) {
    /*
     * Fill cluster-based histogram with corresponding information
     *
     * @param trigger: name of the trigger
     * @param cluster: the EMCal cluster information
     * @param vz: z-position of the vertex
     * @param isPileup: flag event as pileup event
     */
    double data[4] =  {clust->E(), vz, 0, isMinBias ? 1. : 0.};
    char histname[1024];
    sprintf(histname, "hCluster%sHist%s", isCalibrated ? "Calib" : "Uncalib", trigger);
    try{
      fHistos->FillTHnSparse(histname, data);
    } catch (HistoContainerContentException &e){
      std::stringstream errormessage;
      errormessage << "Filling of histogram failed: " << e.what();
      AliError(errormessage.str().c_str());
    }
    if(!isPileup){
      data[2] = 1.;
      try{
        fHistos->FillTHnSparse(histname, data);
      } catch (HistoContainerContentException &e){
        std::stringstream errormessage;
        errormessage << "Filling of histogram failed: " << e.what();
        AliError(errormessage.str().c_str());
      }
    }
  }

  //______________________________________________________________________________
  void AliAnalysisTaskPtEMCalTrigger::FillMCParticleHist(const AliVParticle * const track, double vz, bool isPileup){
    /*
     * Fill histogram for MC-true particles with the information pt, eta and phi
     *
     * @param track: the Monte-Carlo track
     */
    double data[5] = {TMath::Abs(track->Pt()), track->Eta(), track->Phi(), vz, 0.};
    fHistos->FillTHnSparse("hMCtrueParticles", data);
    if(!isPileup){
      data[4] = 1.;
      fHistos->FillTHnSparse("hMCtrueParticles", data);
    }
  }

  //______________________________________________________________________________
  bool AliAnalysisTaskPtEMCalTrigger::IsTrueTrack(const AliVTrack *const track) const{
    /*
     * Check if the track has an associated MC particle, and that the particle is a physical primary
     * In case of data we do not do the selection at that step (always return true)
     *
     * @param track: Track to check
     * @result: true primary track (true or false)
     */
    if(!fMCEvent) return true;
    AliVParticle *mcassociate = fMCEvent->GetTrack(TMath::Abs(track->GetLabel()));
    if(!mcassociate) return false;
    return fMCEvent->IsPhysicalPrimary(TMath::Abs(track->GetLabel()));
  }

  //______________________________________________________________________________
  void AliAnalysisTaskPtEMCalTrigger::AddESDTrackCuts(AliESDtrackCuts* trackCuts) {
    /*
     * Add new track cuts to the task
     *
     * @param trackCuts: Object of type AliESDtrackCuts
     */
    fListTrackCuts->AddLast(new AliEMCalPtTaskTrackSelectionESD(trackCuts));
  }

  //______________________________________________________________________________
  void AliAnalysisTaskPtEMCalTrigger::AddCutsForAOD(AliESDtrackCuts* trackCuts, UInt_t filterbits) {
    /*
     * Add new track cuts to the task
     *
     * @param trackCuts: Object of type AliESDtrackCuts
     */
    fListTrackCuts->AddLast(new AliEMCalPtTaskTrackSelectionAOD(trackCuts, filterbits));
  }


  //______________________________________________________________________________
  TString AliAnalysisTaskPtEMCalTrigger::BuildTriggerString() {
    /*
     * Build trigger string from the trigger maker
     *
     * @return: blank-separated string of fired trigger classes
     */
    AliDebug(1, "trigger checking");
    TString result = "";
    if(HasTriggerType(kJ1)) result += "EJ1 ";
    if(HasTriggerType(kJ2)) result += "EJ2 ";
    if(HasTriggerType(kG1)) result += "EG1 ";
    if(HasTriggerType(kG2)) result += "EG2 ";
    return result;
  }

  //______________________________________________________________________________
  const AliVVertex* AliAnalysisTaskPtEMCalTrigger::GetSPDVertex() const {
    /*
     * Accessor for the SPD vertex, creating transparency for ESDs and AODs
     *
     * @return: the spd vertex
     */
    AliESDEvent *esd = dynamic_cast<AliESDEvent *>(fInputEvent);
    if(esd){
      return esd->GetPrimaryVertexSPD();
    } else {
      AliAODEvent *aod = dynamic_cast<AliAODEvent *>(fInputEvent);
      if(aod){
        return aod->GetPrimaryVertexSPD();
      }
    }
    return NULL;
  }

}

