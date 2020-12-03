R__LOAD_LIBRARY(/w/halla-scifs17exp/parity/disk1/crex_optics/replay/libraries/ParityData/libParity.so)

#include <iostream>
#include <cstdlib>

using namespace std;

Bool_t IsFileExist(const Char_t * fname);

void setup(Int_t runNo=0, Int_t lastevt=-1, TString target="Ca48"){

  char infile[300];
  //static const char* replay_dir_prefix = "./%s";


  // e-p scattering
  Double_t amu = 931.494028*1.e-3;  // amu to GeV
  Double_t he4 = 4*amu;
  Double_t pb208 = 208*amu;  
  Double_t c12 = 12*amu-.511e-3*6;
  Double_t ca40= 40*amu;
  Double_t ca48= 48*amu;
  Double_t H2O =  18*amu;
  //Double_t mass_tg  = he4; // Helium
  Double_t mass_tg = 0;

  if(target=="C12")  mass_tg = c12;
  if(target=="He4")  mass_tg = he4;
  if(target=="Ca40") mass_tg = ca40;
  if(target=="Ca48") mass_tg = ca48;
  if(target=="H2O")  mass_tg = H2O;
  if(target=="Pb208")mass_tg = pb208;

 
  if(runNo<20000){
    THaApparatus* HRSL = new THaHRS("L","Left arm HRS");
    gHaApps->Add( HRSL );
    gHaApps->Add(new ParityData("P","HAPPEX Data"));

    // Electron kinematics
    THaPhysicsModule* EK_l = new THaElectronKine("EK_L",
     					         "Electron kinematics in HRS-L",
					         "L",mass_tg);
    gHaPhysics->Add( EK_l );
    gHaPhysics->Add( new THaGoldenTrack( "L.gold", "HRS-L Golden Track", "L" ));

    THaScalerEvtHandler *lscaler = new THaScalerEvtHandler("Left","HA scaler event type 140");
    //lscaler->SetDebugFile("log/LeftScaler.txt");
    gHaEvtHandlers->Add (lscaler);
  } 
  else{
    THaApparatus* HRSR = new THaHRS("R","Right arm HRS");
    gHaApps->Add( HRSR );
    gHaApps->Add(new ParityData("P","HAPPEX Data"));

    // Electron kinematics
    THaPhysicsModule* EK_r = new THaElectronKine("EK_R",
					         "Electron kinematics in HRS-R",
					         "R",mass_tg);

    gHaPhysics->Add( EK_r );

    gHaPhysics->Add( new THaGoldenTrack( "R.gold", "HRS-R Golden Track", "R" ));

    THaScalerEvtHandler *rscaler = new THaScalerEvtHandler("Right","HA scaler event type 140");
    //rscaler->SetDebugFile("log/RightScaler.txt");
    gHaEvtHandlers->Add (rscaler);
  }

  THaAnalyzer* analyzer = new THaAnalyzer;
  
  char outname[300];
  if(runNo>20000)
    sprintf(outname,"/lustre19/expphy/volatile/halla/parity/crex_optics_rootfiles/prexRHRS_%d_%d.root",runNo, lastevt);
  else
    sprintf(outname,"/lustre19/expphy/volatile/halla/parity/crex_optics_rootfiles/prexLHRS_%d_%d.root",runNo, lastevt);
  analyzer->SetOutFile( outname );
  analyzer->SetCutFile("/w/halla-scifs17exp/parity/disk1/crex_optics/replay/cuts/onlana.cuts");
  if(runNo>20000)
    analyzer->SetOdefFile("/w/halla-scifs17exp/parity/disk1/crex_optics/replay/outputDef/output_R.def");
  else  analyzer->SetOdefFile("/w/halla-scifs17exp/parity/disk1/crex_optics/replay/outputDef/output_L.def");
  analyzer->SetSummaryFile("/w/halla-scifs17exp/parity/disk1/crex_optics/replay/log/summary_example.log"); // optional

  TString oldfilename="";
  int found;
  Int_t FirstEventNum = 0;
  THaRun *oldrun=0, *run;
  Bool_t exit = false;

  if(lastevt>=0) lastevt+= FirstEventNum;

  THaEvent* event = new THaEvent;

  for (Int_t nsplit=0;!exit; nsplit++){
    found = 0;
    if(runNo>20000)
      sprintf(infile,"/cache/halla/happexsp/raw/prexRHRS_%d.dat.%d",runNo,nsplit);
    else
      sprintf(infile,"/cache/halla/happexsp/raw/prexLHRS_%d.dat.%d",runNo,nsplit);

    if( IsFileExist(infile) )
      found = 1;

    if( !found || oldfilename== infile) // did not find the file exit
      {
	exit = true;
      }
    else
      {
	oldfilename = infile;

	if (oldrun)
	  {
	    run = new THaRun(*oldrun);
	    run->SetFilename(infile);
	  }
	else // if this is the first run 
	  {
	    run = new THaRun(infile);
	  }
	
	if(lastevt>=0) run->SetLastEvent(lastevt);
	run->SetFirstEvent(FirstEventNum);
	
	try{
	  analyzer->Process(run);
	}
	catch( exception& e)
	  {
	    cerr << "Unhandled exception during replay: " << e.what() << endl;
	    cerr << "Exiting." << endl;
	    run->Close();
	    break;           
	  }
	
	run->Close();
	if( !oldrun ) oldrun = run;
	
      }//while file exist
  }//nsplit loop

}

Bool_t IsFileExist(const Char_t * fname)
{
  fstream testfile;

  testfile.open(fname, ios_base::in);
  Bool_t isopen = testfile.is_open();
  testfile.close();

  return isopen;
}
