# crex-optics-replay
1. source sourceme.csh to give initial enviroment settings;
2. setup.C is the relay script,in which the default target is Ca48. In order to change the default target, the target should be set in the argument, e.g. setup.C(123,-1,"C12");
3. To submit farm jobs, use replay/submit_run.py:
   1). change "targetfile" to the correct run list;
   2). change "\_target" to the correct target;
