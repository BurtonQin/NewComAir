# NewComAir

Reimplement the sampling part, use apache#34464 to test performance.

1. Implement the clone and sampling part, and measure performance overhead
2. Implement the runtime library
3. Implement the instrumentation part for memory recording, and measure performance overhead
4. Implement other optimization 

Run example:

1. cd apache34464/
2. mkdir build; cd build;
3. make -f ../Makefile install
4. In build: inst_type_id.txt (record the Type and ID of monitored Read/Write Insts)
5. In build: func_name_id.txt (record the FuncName and FuncID)
6. The target.clonesample is copied to targets/
7. ./run.py
8. The results/mem_result.csv records rms,cost
9. The newcomair_123456789 is the raw mem dump file (useless, for debug only)

Change Log:

1. Instruction ID range, defined in include/Common/Constant.h: INVALID = 0, MIN_ID = 1, DELIMIT = INT_MAX
2. read       addr, len, (>0)
3. write      addr, len, (<0)
2. delimitor: 0, 0, DELIMIT
3. cost:      0, cost, 0
4. loop_begin(TODO): addr, len, LOOP_BEGIN 
5. loop_begin(TODO): addr, len, LOOP_END 

TODO:

1. dumpmem: the loop begin and loop end wait to be tested.
2. NewLoopInstrumentor: have not migrate the hoist-sink parts to new code, will finish before 02/11
