#!/usr/bin/env python
# -*- coding: utf-8 -*-

'''
compare runtime of targets
'''

import os
import stat
from subprocess import call
import time
import string

import pandas as pd


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

# Gen sh: target file_name song, exec and record runtime.
# @param target: the exe file, used to test runtime
# @param result: the result file, store runtime results
def run_time_command(target, result):
	
	inputs = []
	exe_times = []
	#for i in range(5000, 55000, 5000):
	for i in range(5000, 55000, 500):
		file_name = generate_input(i)
		command = [target, ' ', file_name, ' ', CONSTANT_SONG]
		with open('test.sh', 'w') as run_script:
			run_script.writelines(command)
			st = os.stat('test.sh')
			os.chmod('test.sh', st.st_mode | stat.S_IEXEC)
		inputs.append(i)
		start = time.time()
		call(['/bin/bash', 'test.sh'])
		exe_times.append('%.3f' % (time.time() - start))

	# delete temp test.sh
	os.remove('test.sh')

	df = pd.DataFrame(data={'inputs': inputs, 'time': exe_times})
	df.to_csv(result, index=False)


if __name__ == '__main__':
	run_time_command('./build/target', 'exe_time.csv')
	run_time_command('./build/target.lalls', 'exe_time.lalls.csv')
	run_time_command('./build/target.new', 'exe_time.new.csv')
