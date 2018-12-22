# Runtime measurements of different versions of targets

Illustration:

0. target.nopass.O0: no pass
1. target.old.O0: original pass
2. target.clonesample.O0: clonesample pass (sample once)
3. target.clonesample.O0-bElseIf: clonesample pass (sample twice)

Steps:

0. rm -rf build/
1. mkdir build; cd build/
2. rm *
3. make -f ../Makefile.nopass OP_LEVEL=0 install
4. rm *
5. make -f ../Makefile.nopass OP_LEVEL=2 install
6. rm *
7. make -f ../Makefile.clonesample OP_LEVEL=0 BELSEIF= install
8. rm *
9. make -f ../Makefile.clonesample OP_LEVEL=2 BELSEIF= install
10. rm *
11. make -f ../Makefile.clonesample OP_LEVEL=0 BELSEIF=-bElseIf install
12. rm *
13. make -f ../Makefile.clonesample OP_LEVEL=2 BELSEIF=-bElseIf install
14. rm *
15. cd ..
16. ./run_exec_time.py

The results are in results.O0/ and results.O2/.
 
Format:
```
inputs,time
54000,2.41494
...
```
