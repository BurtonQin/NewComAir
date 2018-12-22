# Runtime measurements of different versions of targets

Illustration:

0. target.nopass.O0: no pass
1. target.old.O0: Tengfei's pass
2. target.clonesample.O0: clonesample pass (sample once)
3. target.clonesample.O0-bElseIf: clonesample pass (sample twice)
4. O0 can be changed to O2, meaning -O2

Steps:

0. noass.O0:
    ```./run_exe_time.py -nopass```
1. clonesample.O0:
    ```./run_exe_time.py```
2. clonesample.O0-bElseIf:
    ```./run_exe_time.py -bElseif```
3. O2: append -O2 e.g.
    ```./run_exe_time.py -O2```

The results are in results.O0/ and results.O2/.
 
Format:
```
inputs,time
54000,2.41494
...
```
