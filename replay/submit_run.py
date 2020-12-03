#!/apps/python/PRO/bin/python
from subprocess import call
import subprocess
import sys,os,time

def main():
    
    _replaydir="/w/halla-scifs17exp/parity/disk1/crex_optics/"
    _rootout="/lustre19/expphy/volatile/halla/parity/crex_optics_rootfiles/"
    useSWIF=0 #0: uses jsub 1: uses SWIF+jsub

    _runID=1

    targetfile="C12_1.txt"
    _target="C12"
    runfile=open(_replaydir+"Runlist/"+targetfile,"r")
    for line in runfile:
        if (len(line) < 4):
            continue
        _runID=int(line)
        createBatchfile(_replaydir,_rootout,_target,_runID)

        _workflowID="crex_optics_"+str(_runID)

        createJsubfile(_replaydir,_rootout,_workflowID,_runID)

        if useSWIF==1:
           print "SWIF Submitting "+str(_runID)
           call(["swif","add-jsub","-workflow",str(_workflowID),"-create","-script",_rootout+"job_files/"+_workflowID+".xml"])
        elif useSWIF==0:
           print "JSUB submitting "+str(_runID) 
           call(["jsub","-xml",_rootout+"job_files/"+_workflowID+".xml"])

    runfile.close()
    
        
    print "I am all done"


def createBatchfile(replaydir,rootout,target,runID):

    f=open(rootout+"job_files/batch_"+str(runID),"w")
    f.write("#!/bin/tcsh\n")
    f.write("source /u/apps/root/6.18.04/setroot_CUE.csh\n")
    f.write("setenv ANALYZER /w/halla-scifs17exp/parity/disk1/crex_optics/analyzer-1.6.6\n")
    f.write("setenv DB_DIR /w/halla-scifs17exp/parity/disk1/crex_optics/replay/CREX_DB/DB_CREX\n")
    f.write("setenv LD_LIBRARY_PATH ${ANALYZER}:${ROOTSYS}/lib:${LD_LIBRARY_PATH}\n")
    f.write("setenv PATH ${ROOTSYS}/bin:${ANALYZER}:${PATH}\n")

    f.write("analyzer -b -q  '"+replaydir+"replay/setup.C("+str(runID)+",-1,\""+target+"\")'\n")

    f.close()
   
    os.chmod(rootout+"job_files/batch_"+str(runID),0o755)
    return 0

def createJsubfile(replaydir,rootout,workflowID,runID):

    f=open(rootout+"job_files/"+workflowID+".xml","w")
    f.write("<Request>\n")
    f.write("  <Project name=\"prex\"/>\n")
    f.write("  <Track name=\"analysis\"/>\n")
    f.write("  <Name name=\""+workflowID+"\"/>\n")
    f.write("  <OS name=\"centos77\"/>\n")
    f.write("  <CPU core=\"2\"/>\n")
    f.write("  <Memory space=\"2048\" unit=\"MB\"/>\n")
    f.write("  <TimeLimit time=\"5\" unit=\"hours\"/>\n")
    f.write("  <Job>\n")
    f.write("    <Command><![CDATA[\n")
    #f.write("       chmod +x "+rootout+"job_files/batch_"+str(runID)+"\n")
    f.write("       "+rootout+"job_files/batch_"+str(runID)+"\n")
    f.write("    ]]></Command>\n")
    f.write("    <Stdout dest=\""+rootout+"job_output/"+workflowID+".out\"/>\n")
    f.write("    <Stderr dest=\""+rootout+"job_output/"+workflowID+".err\"/>\n")
    f.write("  </Job>\n\n")

    f.write("</Request>\n")
    f.close()
    return 0



    f.close()
    return 0

                    
if __name__ == '__main__':
    main()
                            
