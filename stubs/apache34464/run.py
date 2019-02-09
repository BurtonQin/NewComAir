#!/usr/bin/env python3
# -*- coding: utf-8 -*-

'''
run rms/cost and runtime tests and get results

1. generate input files
2. combine shell command
3. set input range for mem result
4. set input and exec times for runtime result
5. log
'''

import argparse
import os
import pandas as pd
from pathlib import Path
import subprocess
import time
try:
    from subprocess import DEVNULL # py3k
except ImportError:
    import os
    DEVNULL = open(os.devnull, 'wb')


PROJECT_DIR = os.path.dirname(os.path.realpath(__file__))
BELSEIF = ""
#BELSEIF = "-bElseIf"
target = "./targets/target.clonesample" + BELSEIF
counter = 0
#dumpmem = "../../cmake-build-debug/newdumpmem/newdumpmem"
dumpmem = "dumpmem"

CONSTANT_SONG = 'song'

# generate input files in inputs/
def generate_input_files():

    CASE_NAME = './inputs/input_case_{0}.txt'

    for i in range(500, 10500, 500):
        file_name = CASE_NAME.format(i)
        with open(file_name, 'w') as f:
            context = ('a' * i) + CONSTANT_SONG
            f.write(context)

# concat shell command
# cmd: "target ./inputs/input_case_1000.txt song"
def get_command(*params):

    global target
    cmd_str = [str(i) for i in params]
    return [target, *cmd_str, CONSTANT_SONG]

# set input_file size
def get_input_num_range():

    inputs = [i for i in range(500, 5500, 500)]
    sizes = inputs

    return sizes, inputs

# extract input size from input file name
def extract_size(filename):

    fields = filename.split('_')
    size_str = fields[2].split('.')
    return int(size_str[0])

# list all the input files in inputs/
def get_input_file_range():

    size_filepaths = {}

    for dirname, dirnames, filenames in os.walk("./inputs"):
        for filename in filenames:
            #if filename.endswith(".c"):
            #    continue
            size = extract_size(filename)
            size_filepaths[size] = os.path.join(PROJECT_DIR, os.path.join(dirname[2:], filename))
    
    sizes = sorted(size_filepaths.keys())
    inputs = [size_filepaths[size] for size in sizes]

    return sizes, inputs

# set input (num) for runtime test
def get_runtime_num_input():

    input = 1000
    size = input

    return size, input

# set input (file) for runtime test 
def get_runtime_file_input():

    filename = "input_case_1000.txt"

    size = extract_size(filename)

    return size, "/inputs/" + filename

# set exec times in a runtime test
def get_exectimes():

    return 10

# dump mem to logs
def dump_mem():

    global counter 
    subprocess.run(dumpmem + ' newcomair_123456789 ./results/indvar.info > comair_logger_%d' % counter, shell=True)
    counter += 1

# collect logs
def collect_log():

    # init mem result
    mem_result = "./results/mem_result.csv"

    subprocess.run("echo rms,cost > " + mem_result, shell=True)

    subprocess.run("cat $(ls -tr comair_logger_*) | grep -v func_id >> " + mem_result, shell=True)
    
    subprocess.run("rm comair_logger_*", shell=True)

# run rms/cost test
def run_mem(get_input_range):

    print("run_mem:", target)

    my_env = get_env()    

    sizes, inputs = get_input_range()

    for input in inputs:
        command = get_command(input)
        print(command)
        subprocess.run(command, stdout=DEVNULL, env=my_env)
        dump_mem()

    collect_log()
    
# run runtime test
def run_time(get_runtime_input):

    print("run time:", target)

    my_env = get_env()    

    size, input = get_runtime_input()

    exectimes = get_exectimes()

    sizes = [size] * exectimes

    runtime_result = []
    
    for i in range(exectimes):
        command = get_command(input)
        
        start = time.time()
        subprocess.run(command, env=my_env)
        runtime_result.append(time.time() - start)

    df = pd.DataFrame(data={'inputs': sizes, 'time': runtime_result})

    df.to_csv("./results/runtime_result.csv", index=False)
        
    print("avg time: %.5f" % (sum(runtime_result)/len(runtime_result)))

# set sample rate in env
def get_env():

    sample_rate = 100
    if BELSEIF == "-bElseIf":
        sample_rate = 200

    my_env = os.environ.copy()
    my_env['SAMPLE_RATE'] = str(sample_rate)


if __name__ == "__main__":
    
    parser = argparse.ArgumentParser()
    parser.add_argument("-runtime", help="run time result", action="store_true")
    parser.add_argument("-nopass", help="run time result of nopass", action="store_true")
    args = parser.parse_args()

    generate_input_files()
    
    if args.nopass:
        target = "./targets/target"
        run_time(get_runtime_file_input)
    
    elif args.runtime:
        run_time(get_runtime_file_input)

    else:
        run_mem(get_input_file_range)
