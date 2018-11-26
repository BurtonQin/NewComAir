# Runtime measurements of different versions of targets

Illustration:

0. target: no pass
1. target.lalls: original pass
2. target.clonesample: clonesample pass


Steps:

0. rm -rf build/
1. mkdir build; cd build/
2. make -f ../Makefile
3. mv target ../targets.O0/
4. rm *
5. make -f ../Makefile.O2
6. mv target ../targets.O2/
7. rm *
8. make -f ../Makefile.clonesample
9. mv target.clonesample ../targets.O0/
10. rm *
11. make -f ../Makefile.clonesample.O2
12. mv target.clonesample ../targets.O2/
13. rm *
14. cd ..
15. ./run_exec_time.py

The results are in results.O0/ and results.O2/.
 
Format:
```
inputs,time
54000,2.41494
...
```
