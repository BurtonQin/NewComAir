#!/usr/bin/env python3
# -*- coding: utf-8 -*-

'''
compare runtime of targets
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

CASE_NAME = './inputs/input_case_{0}.txt'
CONSTANT_SONG = 'song'

# Gen input str of i length into input_case_{i}.txt.
# input str: a{i}song
def generate_input(i):

	file_name = CASE_NAME.format(i)
	with open(file_name, 'w') as f:
		context = ('a' * i) + CONSTANT_SONG
		f.write(context)

	return file_name


# exec 'target file_name song' and record runtime.
# @param target: the exe file, used to test runtime
# @param result: the result file, store runtime results
def run_time_command(target, result, my_env=None):

	print(target)
	
	inputs = []
	exe_times = []
	i = 5000
	for x in range(20):
		inputs.append(i)
		file_name = generate_input(i)
		command = [target, file_name, CONSTANT_SONG]
		start = time.time()
		if my_env:
			subprocess.run(command, stdout=DEVNULL, env=my_env, check=True)
		else:
			subprocess.run(command, stdout=DEVNULL, check=True)
		exe_times.append('%.5f' % (time.time() - start))

	df = pd.DataFrame(data={'inputs': inputs, 'time': exe_times})
	df.to_csv(result, index=False)

	sum = 0.0
	for t in exe_times:
		sum += float(t)
	print('%.5f' % (sum/len(exe_times)))


# cd build; make -f ../Makefile.clonesample OP_LEVEL=2 ELSEIF=-bElseIf install; cd ..
# @param nopass: nopass instead of clonesample
# @param O: op level
# @param bElseIf: if-elseif-else instead of if-else
def build_and_install(nopass, O, bElseIf):
	
	path = './build'
	Path(path).mkdir(exist_ok=True)
	
	prev_cwd = Path.cwd()
	os.chdir(path)
	if nopass:
		makefile_suffix = '.nopass'
	else:
		makefile_suffix = '.clonesample'
	
	make_O = 'OP_LEVEL=' + str(O)

	if bElseIf:
		make_bElseIf = 'BELSEIF=-bElseIf'
	else:
		make_bElseIf = 'BELSEIF='
		
	subprocess.run(['make', '-f', '../Makefile' + makefile_suffix, make_O, make_bElseIf, 'install'], stdout=DEVNULL, check=True)
	os.chdir(prev_cwd)
	

# run and record time in result, print avg
# @param nopass: nopass instead of clonesample
# @param O: op level
# @param bElseIf: if-elseif-else instead of if-else
def run_get_time(nopass, O, bElseIf):

	target_dir = './targets.O' + str(O)
	result_dir = './results.O' + str(O)

	if nopass:
		target_suffix = 'nopass'
	else:
		target_suffix = 'clonesample'

	target_suffix += '.O' + str(O)

	if not nopass:
		if bElseIf:
			target_suffix += '-bElseIf'
			sample_rate = 200
		else:
			sample_rate = 100

		my_env = os.environ.copy()
		my_env['SAMPLE_RATE'] = str(sample_rate)

		target = os.path.join(target_dir, 'target.' + target_suffix)
		result = os.path.join(result_dir, 'result.' + target_suffix + '.csv')

		run_time_command(target, result, my_env)
	else:
		target = os.path.join(target_dir, 'target.' + target_suffix)
		result = os.path.join(result_dir, 'result.' + target_suffix + '.csv')

		run_time_command(target, result)
	

if __name__ == '__main__':

	parser = argparse.ArgumentParser()
	parser.add_argument("-onlyrun", help="no build, only run", action="store_true")
	parser.add_argument("-nopass", help="use nopass instead of clonesample", action="store_true")
	parser.add_argument("-O", help="opt level", type=int, default=0)
	parser.add_argument("-bElseIf", help="use if-elseif-else instead of if-else", action="store_true")
	args = parser.parse_args()
	
	if (not args.onlyrun):
		build_and_install(args.nopass, args.O, args.bElseIf)
	run_get_time(args.nopass, args.O, args.bElseIf)
