AliAnalysisTask *AddTask_cbaumann_LMEEPbPb2011AODSemiCent(Bool_t runAll=kFALSE,Bool_t setMC=kFALSE,Bool_t getFromAlien=kFALSE, Bool_t PIDbaseline=kFALSE){

  Bool_t bESDANA=kFALSE; //Autodetect via InputHandler
  //get the current analysis manager
  AliAnalysisManager *mgr = AliAnalysisManager::GetAnalysisManager();
  if (!mgr) {
	Error("AddTask_cbaumann_LMEEPbPb2011", "No analysis manager found.");
	return 0;
  }


//  create task and add it to the manager
//	gSystem->AddIncludePath("$ALICE_ROOT/PWGDQ/dielectron/macrosLMEE");


  TString configBasePath("$TRAIN_ROOT/cbaumann_dielectron/");
  TString trainRoot=gSystem->Getenv("TRAIN_ROOT");
  if (trainRoot.IsNull()) configBasePath= "$ALICE_ROOT/PWGDQ/dielectron/macrosLMEE/";


  if (getFromAlien &&
      (!gSystem->Exec("alien_cp alien:///alice/cern.ch/user/c/cbaumann/PWGDQ/dielectron/macrosLMEE/ConfigLMEEPbPb2011AOD.C .")) &&
      (!gSystem->Exec("alien_cp alien:///alice/cern.ch/user/c/cbaumann/PWGDQ/dielectron/macrosLMEE/LMEECutLibAOD.C ."))
     ) {
        configBasePath=Form("%s/",gSystem->pwd());
  }

  TString configFile("ConfigLMEEPbPb2011AOD.C");
  TString configLMEECutLib("LMEECutLibAOD.C");

  TString configFilePath(configBasePath+configFile);
  TString configLMEECutLibPath(configBasePath+configLMEECutLib);

  //AOD Usage currently tested with separate task, to be merged
  if (mgr->GetInputEventHandler()->IsA()==AliAODInputHandler::Class()){
	::Info("AddTaskLMEEPbPb2011", "no dedicated AOD configuration");
  }
  else if (mgr->GetInputEventHandler()->IsA()==AliESDInputHandler::Class()){
	::Info("AddTaskLMEEPbPb2011AOD","switching on ESD specific code");
	bESDANA=kTRUE;
  }


  //Do we have an MC handler?
  Bool_t hasMC=setMC;
  if (AliAnalysisManager::GetAnalysisManager()->GetMCtruthEventHandler()!=0x0)
	hasMC=kTRUE;

  if (!gROOT->GetListOfGlobalFunctions()->FindObject(configLMEECutLib.Data()))
	gROOT->LoadMacro(configLMEECutLibPath.Data());
  if (!gROOT->GetListOfGlobalFunctions()->FindObject(configFile.Data()))
	gROOT->LoadMacro(configFilePath.Data());


  LMEECutLib* cutlib = new LMEECutLib();
  AliAnalysisTaskMultiDielectron *task=new AliAnalysisTaskMultiDielectron("MultiDiEData");
  if (!hasMC) task->UsePhysicsSelection();
  task->SetTriggerMask(AliVEvent::kMB+AliVEvent::kCentral+AliVEvent::kSemiCentral);
  task->SelectCollisionCandidates(AliVEvent::kAny);  
  task->SetEventFilter(cutlib->GetEventCuts(LMEECutLib::kPbPb2011TPCandTOF)); //
	

  //load dielectron configuration file

  //add dielectron analysis with different cuts to the task
  AliDielectron *lowmass2=ConfigLMEEPbPb2011AOD(2,hasMC,bESDANA);
  task->AddDielectron(lowmass2);
  printf("add: %s\n",lowmass2->GetName());


if (PIDbaseline) {
   //obsolete variable, kept for compatibility with existing train setups
}

  mgr->AddTask(task);

  //create output container
  AliAnalysisDataContainer *coutput1 =
	mgr->CreateContainer("cbaumann_LMEEPbPb2011SemiCent_tree",
		TTree::Class(),
		AliAnalysisManager::kExchangeContainer,
		"LMEEoutput.root");

  AliAnalysisDataContainer *cOutputHist1 =
	mgr->CreateContainer("cbaumann_LMEEPbPb2011SemiCent_out",
		TList::Class(),
		AliAnalysisManager::kOutputContainer,
		"LMEEoutput.root");

  AliAnalysisDataContainer *cOutputHist2 =
	mgr->CreateContainer("cbaumann_LMEEPbPb2011SemiCent_CF",
		TList::Class(),
		AliAnalysisManager::kOutputContainer,
		"LMEEoutput.root");

  AliAnalysisDataContainer *cOutputHist3 =
	mgr->CreateContainer("cbaumann_EventStatPbPb2011SemiCent",
		TH1D::Class(),
		AliAnalysisManager::kOutputContainer,
		"LMEEoutput.root");


  mgr->ConnectInput(task,  0, mgr->GetCommonInputContainer());
  mgr->ConnectOutput(task, 0, coutput1 );
  mgr->ConnectOutput(task, 1, cOutputHist1);
  mgr->ConnectOutput(task, 2, cOutputHist2);
  mgr->ConnectOutput(task, 3, cOutputHist3);

  return task;
}
