//*-- Author :    Bob Michaels,  April 2019

//////////////////////////////////////////////////////////////////////////
//
// ParityData
//
// PREX data from spectrometer DAQ.
//   Quartz detectors
//   S0 and S2 scintillators
//   These can be in fastbus (FB) or FADC.
//   Gather data and tracking results for Q2
//   Looking for GEMs ?  it's not here
//
//////////////////////////////////////////////////////////////////////////

//#define WITH_DEBUG 1
#define MAX_SNAP 20

#include "ParityData.h"
#include "THaVarList.h"
#include "THaVar.h"
#include "THaGlobals.h"
#include "THaEvData.h"
#include "THaDetMap.h"
#include "THaAnalyzer.h"
#include "THaString.h"
#include "TMath.h"
#include "TDatime.h"
#include "TH1.h"
#include "VarDef.h"
#include "Module.h"
#include "Fadc250Module.h"
#include <fstream>
#include <iostream>

#define NSTR 12

using namespace std;
using namespace THaString;
using namespace Decoder;

typedef vector<PdataLoc*>::iterator Iter_t;

//_____________________________________________________________________________
ParityData::ParityData( const char* name, const char* descript ) : 
  THaApparatus( name, descript ), dvars(0), IptrFadcL(-1), IptrFadcR(-1)
{
  WindowSize=500; // FADC window (safe max)
  QLfadclo = new Double_t[WindowSize];
  QLfadcup = new Double_t[WindowSize];
  QRfadclo = new Double_t[WindowSize];
  QRfadcup = new Double_t[WindowSize];
  ATLfadc1 = new Double_t[WindowSize];
  ATLfadc2 = new Double_t[WindowSize];
  ATRfadc1 = new Double_t[WindowSize];
  ATRfadc2 = new Double_t[WindowSize];
  trigcnt = new Int_t[12];
  memset(trigcnt, 0, 12*sizeof(Int_t));
  fDebugFile=0;
  // to turn on debugging, uncomment these lines (makes huge output)
  //  fDebugFile = new ofstream;
  // fDebugFile->open("bob.txt");
  Clear();
}

//_____________________________________________________________________________
ParityData::~ParityData()
{ // desctructor
  SetupParData( NULL, kDelete ); 
  if (dvars) delete [] dvars;
  if (QLfadclo) delete [] QLfadclo;
  if (QLfadcup) delete [] QLfadcup;
  if (QRfadclo) delete [] QRfadclo;
  if (QRfadcup) delete [] QRfadcup;
  if (ATLfadc1) delete [] ATLfadc1;
  if (ATLfadc2) delete [] ATLfadc2;
  if (ATRfadc1) delete [] ATRfadc1;
  if (ATRfadc2) delete [] ATRfadc2;
  for( Iter_t p = fWordLoc.begin();  p != fWordLoc.end(); p++ ) delete *p;
  for( Iter_t p = fCrateLoc.begin(); p != fCrateLoc.end(); p++ ) delete *p;
  for( Iter_t p = fDataLocs.begin();  p != fDataLocs.end(); p++ ) delete *p;
}

//_____________________________________________________________________________
void ParityData::Clear( Option_t* opt ) 
{  // to clear the data each event
  evtypebits = 0;
  evtype     = 0;
  for (Int_t i=0; i<Nvars; i++) if(dvars[i]) dvars[i] = 0;
  QLIfadclo=0;   QLIfadcup=0;    
  QRIfadclo=0;   QRIfadcup=0;    
  QLIftimelo=0;  QLIftimeup=0;  
  QRIftimelo=0;  QRIftimeup=0;  
  ATLIfadc1=0;   ATLIfadc2=0;    
  ATRIfadc1=0;   ATRIfadc2=0;    
  ATLIftime1=0;  ATLIftime2=0;   
  ATRIftime1=0;  ATRIftime2=0;  

  QLIfadcS0A=0;   QLIfadcS0B=0;    
  QRIfadcS0A=0;   QRIfadcS0B=0;    
  QLIftimeS0A=0;  QLIftimeS0B=0;  
  QRIftimeS0A=0;  QRIftimeS0B=0;  

  QRIfadcS3=0;   QLIfadcS3=0;    
  QRIftimeS3=0;  QLIftimeS3=0;  

  /*urastXL=0;    urastYL = 0;
  drastXL=0;    drastYL = 0;
  bpmAXPL=0;    bpmAXML =0;
  bpmAYPL=0;    bpmAYML =0;
  bpmEXPL=0;    bpmEXML =0;
  bpmEYPL=0;    bpmEYML =0;*/
  memset(QLfadclo, 0, WindowSize*sizeof(Double_t));
  memset(QLfadcup, 0, WindowSize*sizeof(Double_t));
  memset(QRfadclo, 0, WindowSize*sizeof(Double_t));
  memset(QRfadcup, 0, WindowSize*sizeof(Double_t));
  memset(ATLfadc1, 0, WindowSize*sizeof(Double_t));
  memset(ATLfadc2, 0, WindowSize*sizeof(Double_t));
  memset(ATRfadc1, 0, WindowSize*sizeof(Double_t));
  memset(ATRfadc2, 0, WindowSize*sizeof(Double_t));
  for( Iter_t p = fWordLoc.begin();  p != fWordLoc.end(); p++)  (*p)->Clear();
  for( Iter_t p = fCrateLoc.begin(); p != fCrateLoc.end(); p++) (*p)->Clear();
}

//_____________________________________________________________________________
Int_t ParityData::SetupParData( const TDatime* run_time, EMode mode )
{

  Int_t retval = 0;

// The next several lines illustrates the procedure to add class variables
// like q2L to the list of global varibles, so then P.q2L appears on
// the output.  Note, "dvars", which is automatically produced from
// the database file pardata.map, is also added to the global variables. 
  RVarDef vars[] = {  
    { "q2L",   "Qsq on Left arm",             "q2L" },  
    { "q2R",   "Qsq on Right arm",            "q2R" },  
    { "evtypebits", "event type bit pattern",   "evtypebits" },  
    { "evtype",     "event type from bit pattern", "evtype" },  
// FADC data "Q"=quartz, "AT"=A_T det, "L"/"R"=which HRS, 
// "lo/up" which quartz,  "I"=integrated 
    { "QLIfadclo", "Lower Quartz LHRS Integrated FADC", "QLIfadclo"},
    { "QLIfadcup", "Upper Quartz LHRS Integrated FADC", "QLIfadcup"},
    { "QRIfadclo", "Lower Quartz RHRS Integrated FADC", "QRIfadclo"},
    { "QRIfadcup", "Lower Quartz RHRS Integrated FADC", "QRIfadcup"},
    { "QLIftimelo", "Lower Quartz LHRS time over threshold", "QLIftimelo"},
    { "QLIftimeup", "Upper Quartz LHRS time over threshold", "QLIftimeup"},
    { "QRIftimelo", "Lower Quartz RHRS time over threshold", "QRIftimelo"},
    { "QRIftimeup", "Upper Quartz RHRS time over threshold", "QRIftimeup"},
    { "ATLIfadc1",  "A_T Det LHRS Integrated FADC #1", "ATLIfadc1"},
    { "ATLIfadc2",  "A_T Det LHRS Integrated FADC #2", "ATLIfadc2"},
    { "ATRIfadc1",  "A_T Det RHRS Integrated FADC #1", "ATRIfadc1"},
    { "ATRIfadc2",  "A_T Det RHRS Integrated FADC #2", "ATRIfadc2"},
    { "ATLIftime1", "A_T Det LHRS time over threshold", "ATLIftime1"},
    { "ATLIftime2", "A_T Det LHRS time over threshold", "ATLIftime2"},
    { "ATRIftime1", "A_T Det RHRS time over threshold", "ATRIftime1"},
    { "ATRIftime2", "A_T Det RHRS time over threshold", "ATRIftime2"},

    { "QLIfadcS3", "S3 LHRS Integrated FADC", "QLIfadcS3"},
    { "QRIfadcS3", "S3 RHRS Integrated FADC", "QRIfadcS3"},
    { "QLIfadcS0A", "S0A LHRS Integrated FADC", "QLIfadcS0A"},
    { "QLIfadcS0B", "S0B LHRS Integrated FADC", "QLIfadcS0B"},
    { "QRIfadcS0A", "S0A RHRS Integrated FADC", "QRIfadcS0A"},
    { "QRIfadcS0B", "S0B RHRS Integrated FADC", "QRIfadcS0B"},
    { "QLIftimeS0A", "S0A LHRS time over threshold", "QLIftimeS0A"},
    { "QLIftimeS0B", "S0B LHRS time over threshold", "QLIftimeS0B"},
    { "QRIftimeS0A", "S0A RHRS time over threshold", "QRIftimeS0A"},
    { "QRIftimeS0B", "S0B RHRS time over threshold", "QRIftimeS0B"},
    { "QLIftimeS3", "S3 LHRS time over threshold", "QLIftimeS3"},
    { "QRIftimeS3", "S3 RHRS time over threshold", "QRIftimeS3"},
    /*{ "urastXL", "U raster X LHRS", "urastXL"},
    { "urastYL", "U raster Y LHRS", "urastYL"},
    { "drastXL", "D raster X LHRS", "drastXL"},
    { "drastYL", "D raster Y LHRS", "drastYL"},
    { "bpmAXPL", "BPMA Xplus LHRS", "bpmAXPL"},
    { "bpmAXML", "BPMA Xminus LHRS", "bpmAXML"},
    { "bpmAYPL", "BPMA Yplus LHRS", "bpmAYPL"},
    { "bpmAYML", "BPMA Yminus LHRS", "bpmAYML"},
    { "bpmEXPL", "BPME Xplus LHRS", "bpmEXPL"},
    { "bpmEXML", "BPME Xminus LHRS", "bpmEXML"},
    { "bpmEYPL", "BPME Yplus LHRS", "bpmEYPL"},
    { "bpmEYML", "BPME Yminus LHRS", "bpmEYML"},*/
    { 0 }
  };

  if( mode != kDefine || !fIsSetup )
    retval = DefineVarsFromList( vars, mode );

  fIsSetup = ( mode == kDefine );
   
  fCrateLoc.clear();   
  fWordLoc.clear();   

  ifstream pardatafile;

  const char* const here = "SetupParData()";
  const char* name = GetDBFileName();

  TDatime date;
  if( run_time ) date = *run_time;
  vector<string> fnames = GetDBFileList( name, date, Here(here));
  // always look for 'pardata.map' in the current directory first.
  fnames.insert(fnames.begin(),string("pardata.map"));
  if( !fnames.empty() ) {
    vector<string>::iterator it = fnames.begin();
    do {
#ifdef WITH_DEBUG
      if( fDebug>0 ) {
	cout << "<" << Here(here) << ">: Opening database file " << *it;
      }
#endif
      pardatafile.clear();  // Forget previous failures before attempt
      pardatafile.open((*it).c_str());

#ifdef WITH_DEBUG
      if (pardatafile) {
	cout << "Opening file "<<*it<<endl;
      } else {
        cout << "cannot open file "<<*it<<endl;
      }
      if( fDebug>0 ) {
	if( !pardatafile ) cout << " ... failed" << endl;
	else               cout << " ... ok" << endl;
      }
#endif
    } while ( !pardatafile && ++it != fnames.end() );
  }

  if( fnames.empty() || !pardatafile ) {
    cout << "ERROR:: ParityData: File db_"<<name<<".dat not found."<<endl;
    return -1;
  }

  string sinput;
  const string comment = "#";
  while (getline(pardatafile, sinput)) {
#ifdef WITH_DEBUG
       cout << "sinput "<<sinput<<endl;
#endif
       vector<string> strvect( vsplit(sinput) );
       if (strvect.size() < 5 || strvect[0] == comment) continue;
       Int_t crate = (Int_t)atoi(strvect[2].c_str());  // crate #
       PdataLoc *b = 0;
       if (strvect[1] == "crate") {  // Crate data ?
           Int_t slot = (Int_t)atoi(strvect[3].c_str());
           Int_t chan = (Int_t)atoi(strvect[4].c_str());
           b = new PdataLoc(strvect[0].c_str(), crate, slot, chan); 
           fCrateLoc.push_back(b);
           if(strvect[0] == "fadcL") IptrFadcL=fCrateLoc.size()-1;
           if(strvect[0] == "fadcR") IptrFadcR=fCrateLoc.size()-1;
       } else {         // Data is relative to a header
           UInt_t header = header_str_to_base16(strvect[3].c_str());
           Int_t skip = (Int_t)atoi(strvect[4].c_str());
           b = new PdataLoc(strvect[0].c_str(), crate, header, skip);
           fWordLoc.push_back(b);
       }
       DefineChannel(b,mode);

  }

// Bit pattern for trigger definition
   for (UInt_t i = 0; i < bits.GetNbits(); i++) {
// LHRS event type bits 
fCrateLoc.push_back(new PdataLoc(Form("Lbit%d",i+1), 5, (Int_t) 16, i));
// RHRS event type bits 
fCrateLoc.push_back(new PdataLoc(Form("Rbit%d",i+1), 6, (Int_t) 16, 48+i));

   }



    PrintMap();
    Print();

    fIsSetup = ( mode == kDefine );

    if( mode != kDefine ) {   // cleanup the dynamically built list
      for (unsigned int i=0; i<fCrateLoc.size(); i++) DefineChannel(fCrateLoc[i],mode);
      for (unsigned int i=0; i<fWordLoc.size(); i++) DefineChannel(fWordLoc[i],mode);
      for( Iter_t p = fDataLocs.begin();  p != fDataLocs.end(); p++ )  delete *p;
 
      GloVars.clear();
      return retval;
    }

  dvars = new Double_t[GloVars.size()];    

  return retval;
}

//_____________________________________________________________________________
void ParityData::PrintMap() {
  cout << "Map for Parity Data "<<endl;
  for( Iter_t p = fCrateLoc.begin(); p != fCrateLoc.end(); p++) {
    PdataLoc *dataloc = *p;
    dataloc->Print();
  }
  for (Iter_t p = fWordLoc.begin(); p != fWordLoc.end(); p++) {
    PdataLoc *dataloc = *p;
    dataloc->Print();
  }  
  
}

//_____________________________________________________________________________
PdataLoc* ParityData::DefineChannel(PdataLoc *b, EMode mode, const char* desc) {
  string nm(fPrefix + b->name);
  if (mode==kDefine) {
    if (gHaVars) {
       gHaVars->Define(nm.c_str(),desc,b->rdata[0],&(b->ndata));
       GloVars.push_back(nm);
       fDataLocs.push_back(b);  
    }
  } else {
    if (gHaVars) gHaVars->RemoveName(nm.c_str());
  }
  return b;
}
  
//_____________________________________________________________________________
Int_t ParityData::End( THaRunBase* run ) 
{
  WriteHist();
  return 0;
}

//_____________________________________________________________________________
  void ParityData::WriteHist()
{
  for (UInt_t i = 0; i < hist.size(); i++) hist[i]->Write();
}

//_____________________________________________________________________________
  void ParityData::BookHist()
{
  hist.clear();
  for (Int_t i=0; i<MAX_SNAP; i++) {
    char cname[50];  char ctitle[80]; 
    sprintf(cname,"snap%d",i+1);
    sprintf(ctitle,"Snapshot for event %d",i+1);    
    hist.push_back(new TH1F(cname,ctitle,440,-20,420));
  }
  Ltdc = new TH1F("Ltdc","LHRS TDC for event type bits",400,0,10000);
  Rtdc = new TH1F("Rtdc","RHRS TDC for event type bits",400,0,10000);
}


//_____________________________________________________________________________
THaAnalysisObject::EStatus ParityData::Init( const TDatime& run_time ) 
{

  fStatus = kNotinit;
  MakePrefix();
  BookHist();
  fStatus = static_cast<EStatus>( SetupParData( &run_time ) );

  return fStatus;
}




//_____________________________________________________________________________
Int_t ParityData::Decode(const THaEvData& evdata)
{

  Int_t i;
  Int_t ldebug=0;
  Int_t run_number = evdata.GetRunNum();
  Clear();

  if(ldebug&&fDebugFile) *fDebugFile << "Entering decode. Run number "<<run_number<<"  Event number "<<evdata.GetEvNum()<<endl;

// Fastbus data (crate, slot, chan)
  for( Iter_t p = fCrateLoc.begin(); p != fCrateLoc.end(); p++) {
    PdataLoc *dataloc = *p;
    if ( dataloc->IsSlot() ) {  
      for (i = 0; i < evdata.GetNumHits(dataloc->crate, 
		         dataloc->slot, dataloc->chan); i++) {
	dataloc->Load(evdata.GetData(dataloc->crate, dataloc->slot, 
				     dataloc->chan, i));
      }
    }
  }

// Data that is marked by a crate, header, and how far to skip from header
  for (i = 0; i < evdata.GetEvLength(); i++) {
    for (Iter_t p = fWordLoc.begin(); p != fWordLoc.end(); p++) {
      PdataLoc *dataloc = *p;
      if ( dataloc->DidLoad() || dataloc->IsSlot() ) continue;
      if ( evdata.InCrate(dataloc->crate, i) ) {
        if ((UInt_t)evdata.GetRawData(i) == dataloc->header) {
            dataloc->Load(evdata.GetRawData(i + dataloc->ntoskip));
        }
      }
    }
  }

  evtype = evdata.GetEvType();   // CODA event type 

  if (ldebug) cout << "Into Decode "<<evtype<<"  "<<fCrateLoc.size()<<"   "<<fWordLoc.size()<<endl;

  for( Iter_t p = fCrateLoc.begin(); p != fCrateLoc.end(); p++) {

    PdataLoc *dataloc = *p;

// bit pattern of triggers
    for (UInt_t i = 0; i < bits.GetNbits(); i++) {
      if ( dataloc->ThisIs(Form("Lbit%d",i+1)) ) TrigBits(0,i+1,dataloc);
      if ( dataloc->ThisIs(Form("Rbit%d",i+1)) ) TrigBits(1,i+1,dataloc);
    }
  }

// Process the global variables.

  for (UInt_t i=0; i<GloVars.size(); i++) {
    dvars[i] = 1.0*fDataLocs[i]->Get();  // the 1.0* safely converts 
                                         // int to double
  }                                      

  if (IptrFadcL > 0) DecodeFadc(ILEFT,  IptrFadcL, evdata);
  if (IptrFadcR > 0) DecodeFadc(IRIGHT, IptrFadcR, evdata);


  DoBpm();

  DoKine();

  if (ldebug) Print();

  return 0;
}


//_____________________________________________________________________________
Int_t ParityData::DoBpm( ) {
  // Nothing here yet.
  return 1;
}


//_____________________________________________________________________________
Int_t ParityData::DoKine( ) {
// Calculate Q^2 
// I realized later that this routine obtains
// the same result as THaElectronKine, but this is 
// a good cross check.


  Int_t ldebug=0;

  if(ldebug&&fDebugFile) *fDebugFile << "entering DoKine "<<endl;

  Double_t pL,thL,phiL;
  Double_t pR,thR,phiR;
  THaVar *pvar;

//  Double_t ebeam = 0.95;     // E_beam - < dE/dx >

 // Double_t theta_L = 4.74;    // central angle, degrees
//  Double_t theta_R = -4.74;    // central angle, degrees
//  Double_t thoff_L = 0;      // offset to play with
//  Double_t thoff_R = 0;      // offset "   "    "
   //for crex 12/12/2019
   Double_t  ebeam = 2.18352;
   Double_t theta_L = 4.74;    // central angle, degrees
   Double_t theta_R = -4.74;    // central angle, degrees
   Double_t thoff_L = 0;      // offset to play with
   Double_t thoff_R = 0;      // offset "   "    "

  Double_t pscale  = 1.00;    // momentum scale factor
                              // to play with

  Double_t pi = 3.1415926;
  Double_t thL0,thR0,costhL,costhR,sinthL,sinthR;

  thL0 = (theta_L+thoff_L)*pi/180;
  costhL = TMath::Cos(thL0);
  sinthL = TMath::Sin(thL0);
  thR0 = (theta_R+thoff_R)*pi/180;
  costhR = TMath::Cos(thR0);
  sinthR = TMath::Sin(thR0);

  pL   = -999;
  thL  = -999;
  phiL = -999;
  pR   = -999;
  thR  = -999;
  phiR = -999;
  Int_t okL = 0;  
  Int_t okR = 0;

// Must have the THaGoldenTrack module invoked
  
  pvar = gHaVars->Find("L.gold.ok");
  if (pvar) okL = (Int_t) pvar->GetValue();
  if (ldebug&&fDebugFile) *fDebugFile << "qsq:  L.gold.ok ? "<<okL<<"   "<<pvar<<endl;
  if (okL) {
    pvar = gHaVars->Find("L.gold.p");
    if (pvar) pL = pvar->GetValue();
    pvar = gHaVars->Find("L.gold.th");
    if (pvar) thL = pvar->GetValue();
    pvar = gHaVars->Find("L.gold.ph");
    if (pvar) phiL = pvar->GetValue();
  }
  if (ldebug&&fDebugFile) *fDebugFile << "pL "<<pL<<"   "<<thL<<"   "<<phiL<<endl;

  pvar = gHaVars->Find("R.gold.ok");
  if (pvar) okR = (Int_t) pvar->GetValue();
  if (okR) {
    pvar = gHaVars->Find("R.gold.p");
    if (pvar) pR = pvar->GetValue();
    pvar = gHaVars->Find("R.gold.th");
    if (pvar) thR = pvar->GetValue();
    pvar = gHaVars->Find("R.gold.ph");
    if (pvar) phiR = pvar->GetValue();
  }  

  q2L = -999;

  if (pL > -999 && thL > -999 && phiL > -999 &&
      pL < 1e32 && thL < 1e32 && phiL < 1e32) {
    q2L = 2*ebeam*(pscale*pL)*(1-((costhL-sinthL*phiL)/TMath::Sqrt(1+thL*thL+phiL*phiL)));
  }

  q2R = -999;
  if (pR > -999 && thR > -999 && phiR > -999 &&
      pR < 1e32 && thR < 1e32 && phiR < 1e32) {
// note : sign convention requires costhR + sinthR*phiR 
//                                    not -
    q2R = 2*ebeam*(pscale*pR)*(1-((costhR+sinthR*phiR)/TMath::Sqrt(1+thR*thR+phiR*phiR)));
  }

  return 1;

}


//_________________________________fDebugFile____________________________________________
Int_t ParityData::DecodeFadc(Int_t iarm, Int_t iptr, const THaEvData& evdata) {

  // Decoding of the FADC data for Parity-related signals.

  Fadc250Module *fadc=NULL;
  Int_t fadc_mode, num_fadc_events;
  Bool_t raw_mode;

  Int_t evnum = evdata.GetEvNum();
  Int_t run_number = evdata.GetRunNum(); 
// channels where to find data; it might be better to not use hard-coded values.

  Int_t IQLIfadcS0A=0;    // L-HRS S0A, S0B
  Int_t IQLIfadcS0B=1;    
  Int_t IQRIfadcS0A=0;    // R-HRS S0A, S0B 
  Int_t IQRIfadcS0B=1;   
  Int_t IQLIfadcS3=2;    
  Int_t IQRIfadcS3=2;   

  Int_t IQLIftimeS0A=0;   // L-HRS time over thr,S0A, S0B
  Int_t IQLIftimeS0B=1;   
  Int_t IQRIftimeS0A=0;   // R-HRS time over thr,S0A, S0B
  Int_t IQRIftimeS0B=1;  

  Int_t IQLIftimeS3=2;   
  Int_t IQRIftimeS3=2;  
  Int_t IQLIfadclo=4;    // L-HRS lower, upper Quartz
  Int_t IQLIfadcup=3;    
  Int_t IQRIfadclo=4;    // R-HRS lower, upper Quartz 
  Int_t IQRIfadcup=3;   
  Int_t IQLIftimelo=4;   // L-HRS time over thr, L-HRS lower, upper
  Int_t IQLIftimeup=3;   
  Int_t IQRIftimelo=4;   // R-HRS time over thr, L-HRS lower, upper
  Int_t IQRIftimeup=3;  

  Int_t IATLIfadc1=5;    // integrated adc L-HRS AT#1 & 2
  Int_t IATLIfadc2=6;    
  Int_t IATRIfadc1=5;    // integrated adc R-HRS AT#1 & 2
  Int_t IATRIfadc2=6;    
  Int_t IATLIftime1=5;   // time over thr, L-HRS AT#1 & 2
  Int_t IATLIftime2=6;   
  Int_t IATRIftime1=5;   // time over thr, R-HRS AT#1 & 2
  Int_t IATRIftime2=6;   
  /*Int_t IurastXL=0;    
  Int_t IurastYL=1;
  Int_t IdrastXL=0;    
  Int_t IdrastYL=1;
  Int_t IbpmAXML=0;    
  Int_t IbpmAXPL=1;

  Int_t IbpmAYML=0;
  Int_t IbpmAYPL=1;

  Int_t IbpmEXML=0;
  Int_t IbpmEXPL=1;

  leInt_t IbpmEYML=0;
  Int_t IbpmEYPL=1;*/


  Int_t ldebug=1;

  Int_t crate, slot;

  crate=-1;  slot=-1;
  if(ldebug&&fDebugFile) *fDebugFile << "ye edit "<<"crate"<<"  "<<crate<<"slot"<<"  "<<slot<<"fcrateloc size"<<"  "<< fCrateLoc.size()<<endl;
  if (iptr >= 0 && iptr < fCrateLoc.size()) {
    crate = fCrateLoc[iptr]->crate;  // might be a way to get crate, slot  
    slot = fCrateLoc[iptr]->slot;
    if (fDebugFile) *fDebugFile << "fadc_crate "<<crate<<" slot "<<slot<<"why"<<endl; 
 }
  if (run_number > 20000) {
      crate=20;  slot=6; //RHRS 
     // pedfact = 41./4.;  // = (NSA+NSB)/NPED
  } else {
      crate=31;  slot=5; //LHRS
    //  pedfact = 34./4.; // = (NSA+NSB)/NPED
  }
  Int_t mychannel=0; // an example
  if (fDebugFile) *fDebugFile << "fadc_crate "<<crate<<" slot "<<slot<<endl;

  if(ldebug&&fDebugFile) *fDebugFile << "Decode FADC "<<iarm<<"  "<<iptr<<"  "<<crate<<"   "<<slot<<endl;

   fadc = dynamic_cast <Fadc250Module*> (evdata.GetModule(crate, slot));
   if(ldebug&&fDebugFile) *fDebugFile << "fadc ptr "<<fadc<<endl;	

// You can either get the FADC data from "evdata" or directly from the "fadc" object.

   if (fadc) {

     fadc_mode = fadc->GetFadcMode();
     num_fadc_events = fadc->GetNumFadcEvents(0);
     raw_mode  = ((fadc_mode == 1) || (fadc_mode == 8) || (fadc_mode == 10));
     Int_t numSamp = fadc->GetNumEvents(kSampleADC,0);
     Int_t numPulInt = fadc->GetNumEvents(kPulseIntegral,0);
     Int_t numPulTime = fadc->GetNumEvents(kPulseTime,0);
     if(ldebug&&fDebugFile) {
       *fDebugFile << "fadc_mode "<<fadc_mode<<"   num events "<<num_fadc_events<<endl;
       if(raw_mode) {
          *fDebugFile << "Fadc is in raw mode "<<endl;
          *fDebugFile << "Num events  : samples  "<<numSamp<<"   integrals "<<numPulInt<<"   time "<<numPulTime<<endl;
          *fDebugFile << "num samples on channel "<<mychannel<<"   "<<evdata.GetNumEvents(kSampleADC, crate, slot, mychannel)<<endl;
          for (Int_t ihit = 0; ihit< evdata.GetNumEvents(kSampleADC, crate, slot, mychannel); ihit++) {
            if(ihit>0 && (ihit%8==0)) *fDebugFile << endl;
            *fDebugFile << "  Sample "<<ihit<<"   "<<evdata.GetData(kSampleADC, crate, slot, mychannel, ihit);
	  }
          for (Int_t chan = 0; chan<16; chan++) {
	    *fDebugFile << "==== FADC channel ==== "<<chan<<endl;
            for (Int_t ihit = 0; ihit< evdata.GetNumEvents(kPulseIntegral, crate, slot, chan); ihit++) {
              *fDebugFile << "    Pulse integral "<<ihit<<"   "<<evdata.GetData(kPulseIntegral, crate, slot, chan, ihit)<< "   time "<<evdata.GetData(kPulseTime, crate, slot, chan, ihit)<< "   peak "<<evdata.GetData(kPulsePeak, crate, slot, chan, ihit)<<endl;
	    }
	  }
       } // if raw mode
     }  // if ldebug

// Snapshot histograms for a few events
     if (evnum >= 0 && evnum < MAX_SNAP) {
       for (Int_t ihit = 0; ihit< evdata.GetNumEvents(kSampleADC, crate, slot, mychannel); ihit++) {
	 hist[evnum]->Fill(ihit,evdata.GetData(kSampleADC, crate, slot, mychannel, ihit));
       }
     }

   }  // if fadc

   if (crate >=0 && slot >= 0) {
     // Here IQLIfadclo ifDebugFiles a channel number for QLIfadclo,
     // so the channel number is hard coded.  
     // Also it's assumed that only one crate & slot is used.
     // Obviously this can be improved, it's a first version.

    QLIfadclo = evdata.GetData(kPulseIntegral,crate,slot,IQLIfadclo,0); 
    QLIfadcup = evdata.GetData(kPulseIntegral,crate,slot,IQLIfadcup,0); 
    QRIfadclo = evdata.GetData(kPulseIntegral,crate,slot,IQRIfadclo,0); 
    QRIfadcup = evdata.GetData(kPulseIntegral,crate,slot,IQRIfadcup,0); 
    ATLIfadc1 = evdata.GetData(kPulseIntegral,crate,slot,IATLIfadc1,0);
    ATLIfadc2 = evdata.GetData(kPulseIntegral,crate,slot,IATLIfadc2,0);    
    ATRIfadc1 = evdata.GetData(kPulseIntegral,crate,slot,IATRIfadc1,0);
    ATRIfadc2 = evdata.GetData(kPulseIntegral,crate,slot,IATRIfadc2,0);

    QLIfadcS0A = evdata.GetData(kPulseIntegral,crate,slot,IQLIfadcS0A,0); 
    QLIfadcS0B = evdata.GetData(kPulseIntegral,crate,slot,IQLIfadcS0B,0); 
    QRIfadcS0A = evdata.GetData(kPulseIntegral,crate,slot,IQRIfadcS0A,0); 
    QRIfadcS0B = evdata.GetData(kPulseIntegral,crate,slot,IQRIfadcS0B,0); 
 
    QLIfadcS3 = evdata.GetData(kPulseIntegral,crate,slot,IQLIfadcS3,0); 
    QRIfadcS3 = evdata.GetData(kPulseIntegral,crate,slot,IQRIfadcS3,0); 

    QLIftimelo = evdata.GetData(kPulseTime,crate,slot,IQLIftimelo,0);
    QLIftimeup = evdata.GetData(kPulseTime,crate,slot,IQLIftimeup,0);
    QRIftimelo = evdata.GetData(kPulseTime,crate,slot,IQRIftimelo,0);
    QRIftimeup = evdata.GetData(kPulseTime,crate,slot,IQRIftimeup,0);
    ATLIftime1 = evdata.GetData(kPulseTime,crate,slot,IATLIftime1,0);
    ATLIftime2 = evdata.GetData(kPulseTime,crate,slot,IATLIftime2,0);
    ATRIftime1 = evdata.GetData(kPulseTime,crate,slot,IATRIftime1,0);
    ATRIftime2 = evdata.GetData(kPulseTime,crate,slot,IATRIftime2,0);

    QLIftimeS0A = evdata.GetData(kPulseTime,crate,slot,IQLIftimeS0A,0);
    QLIftimeS0B = evdata.GetData(kPulseTime,crate,slot,IQLIftimeS0B,0);
    QRIftimeS0A = evdata.GetData(kPulseTime,crate,slot,IQRIftimeS0A,0);
    QRIftimeS0B = evdata.GetData(kPulseTime,crate,slot,IQRIftimeS0B,0);

    QLIftimeS3 = evdata.GetData(kPulseTime,crate,slot,IQLIftimeS3,0);
    QRIftimeS3 = evdata.GetData(kPulseTime,crate,slot,IQRIftimeS3,0);

  /*  urastXL = evdata.GetData(kPulseIntegral,crate,slot,IurastXL,0);
    urastYL = evdata.GetData(kPulseIntegral,crate,slot,IurastYL,0);
    drastXL = evdata.GetData(kPulseIntegral,crate,slot,IdrastXL,0);
    drastYL = evdata.GetData(kPulseIntegral,crate,slot,IdrastYL,0);
    bpmAXML = evdata.GetData(kPulseIntegral,crate,slot,IbpmAXML,0);
    bpmAXPL = evdata.GetData(kPulseIntegral,crate,slot,IbpmAXPL,0);
    bpmAYML = evdata.GetData(kPulseIntegral,crate,slot,IbpmAYML,0);
    bpmAYPL = evdata.GetData(kPulseIntegral,crate,slot,IbpmAYPL,0);
    bpmEXML = evdata.GetData(kPulseIntegral,crate,slot,IbpmEXML,0);
    bpmEXPL = evdata.GetData(kPulseIntegral,crate,slot,IbpmEXPL,0);
    bpmEYML = evdata.GetData(kPulseIntegral,crate,slot,IbpmEYML,0);
    bpmEYPL = evdata.GetData(kPulseIntegral,crate,slot,IbpmEYPL,0);*/

    if(ldebug&&fDebugFile) {
      if(iarm==ILEFT) {  // basically just an example
        *fDebugFile << "Quartz FADC pulse-integral data, L-HRS "<<endl;
        *fDebugFile <<  QLIfadclo << "  "<<QLIfadcup<<"   "<<QRIfadclo<<"   "<<QRIfadcup<<endl;
        *fDebugFile << "Quartz FADC time over threshold, L-HRS "<<endl;
        *fDebugFile <<  QLIftimelo << "  "<<QLIftimeup<<"   "<<QRIftimelo<<"   "<<QRIftimeup<<endl;
        /**fDebugFile << "Raster FADC pulse-integral data, L-HRS "<<endl;
        *fDebugFile <<  urastXL << "  "<<urastYL<<"   "<<urastXL<<"   "<<urastYL<<endl;
        *fDebugFile << "BPMA FADC pulse-integral data, L-HRS "<<endl;
        *fDebugFile <<  bpmAXML << "  "<<bpmAXPL<<"   "<<bpmAYML<<"   "<<bpmAYPL<<endl;
        *fDebugFile << "BPME FADC pulse-integral data, L-HRS "<<endl;
        *fDebugFile <<  bpmEXML << "  "<<bpmEXPL<<"   "<<bpmEYML<<"   "<<bpmEYPL<<endl;*/
      }
    }

   }

  return 1;
}

//_____________________________________________________________________________
void ParityData::Print( Option_t* opt ) const {
// Dump the data for purpose of debugging.
  if (!fDebugFile) return;
  *fDebugFile << "Dump of ParityData "<<endl;
  *fDebugFile << "event pattern bits : ";
   for (UInt_t i = 0; i < bits.GetNbits(); i++) 
    *fDebugFile << " "<<i<<" = "<< bits.TestBitNumber(i)<<"  | ";
  *fDebugFile << endl;
  *fDebugFile << "Accepted Trigger Counts "<<endl;
  for (Int_t i = 0; i < 12; i++) {
    *fDebugFile << "     Trig "<<i+1<<"   count = "<<trigcnt[i]<<endl;
  }
  *fDebugFile << "Number of global variables "<<GloVars.size()<<endl;
  for (UInt_t i=0; i<GloVars.size(); i++) {
    *fDebugFile << "variable: "<<i<<"   "<<GloVars[i];
    if (dvars) *fDebugFile << "   =  "<<dvars[i];
    *fDebugFile << endl;
  }
}


//_____________________________________________________________________________
vector<string> ParityData::vsplit(const string& s) {
// split a string into whitespace-separated strings
  vector<string> ret;
  typedef string::size_type ssiz_t;
  ssiz_t i = 0;
  while ( i != s.size()) {
    while (i != s.size() && isspace(s[i])) ++i;
      ssiz_t j = i;
      while (j != s.size() && !isspace(s[j])) ++j;
      if (i != j) {
         ret.push_back(s.substr(i, j-i));
         i = j;
      }
  }
  return ret;
}

//_____________________________________________________________________________
UInt_t ParityData::header_str_to_base16(const char* hdr) {
// Utility to convert string header to base 16 integer
  static const char chex[] = "0123456789abcdef";
  if( !hdr ) return 0;
  const char* p = hdr+strlen(hdr);
  UInt_t result = 0;  UInt_t power = 1;
  while( p-- != hdr ) {
    const char* q = strchr(chex,tolower(*p));
    if( q ) {
      result += (q-chex)*power; 
      power *= 16;
    }
  }
  return result;
};

//_____________________________________________________________________________
void ParityData::TrigBits(UInt_t hrsnum, UInt_t ibit, PdataLoc *dataloc) {
// Figure out which triggers got a hit.  These are multihit TDCs, so we
// need to sort out which hit we want to take by applying cuts.
// This is done somewhere else in the analyzer, too.

  if( ibit >= kBitsPerByte*sizeof(UInt_t) ) return; //Limit of evtypebits
  bits.ResetBitNumber(ibit);

  static const UInt_t cutlo = 200;
  static const UInt_t cuthi = 2500;  // may need adjustment of cuts

  if (fDebugFile) *fDebugFile << " HRS  "<<hrsnum<<" Bit  TDC num hits "<<dataloc->NumHits()<<endl;
 for (int ihit = 0; ihit < dataloc->NumHits(); ihit++) {
      if (fDebugFile) *fDebugFile << "TDC data " << ibit<<"  "<<dataloc->Get(ihit)<<endl;
      if(hrsnum==0) Ltdc->Fill(dataloc->Get(ihit));
      if(hrsnum==1) Rtdc->Fill(dataloc->Get(ihit));

    if (dataloc->Get(ihit) > cutlo && dataloc->Get(ihit) < cuthi) {
      bits.SetBitNumber(ibit);
      evtypebits |= BIT(ibit);
      if(ibit>=0 && ibit<12) trigcnt[ibit]++;
    }
  }

}

//_____________________________________________________________________________
ClassImp(ParityData)

