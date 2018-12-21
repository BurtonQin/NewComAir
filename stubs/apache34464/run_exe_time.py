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
from pylab import *
from scipy.optimize import curve_fit


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
	#for i in range(5000, 55000, 1000):
	for x in range(20):
		i = 5000
		file_name = generate_input(i)
		command = [target, ' ', file_name, ' ', CONSTANT_SONG]
		with open('test.sh', 'w') as run_script:
			run_script.writelines(command)
			st = os.stat('test.sh')
			os.chmod('test.sh', st.st_mode | stat.S_IEXEC)
		inputs.append(i)
		start = time.time()
		call(['/bin/bash', 'test.sh'])
		exe_times.append('%.5f' % (time.time() - start))

	# delete temp test.sh
	os.remove('test.sh')

	df = pd.DataFrame(data={'inputs': inputs, 'time': exe_times})
	df.to_csv(result, index=False)

def polyfit(x, y, degree):
    results = {}
    coeffs = np.polyfit(x, y, degree)
    results['polynomial'] = coeffs.tolist()

    # r-squared
    p = np.poly1d(coeffs)
    # fit values, and mean
    yhat = p(x)                         # or [p(z) for z in x]
    ybar = np.sum(y)/len(y)          # or sum(y)/len(y)
    ssreg = np.sum((yhat-ybar)**2)   # or sum([ (yihat - ybar)**2 for yihat in yhat])
    sstot = np.sum((y - ybar)**2)    # or sum([ (yi - ybar)**2 for yi in y])
    results['determination'] = ssreg / sstot  # 准确率
    return results


def calculate_curve_fit(result):
    """
    calculate the expression of target function
    :return:
    """
    df = pd.read_csv(result)
    xdata = df[['inputs']].apply(pd.to_numeric)
    ydata = df[['time']].apply(pd.to_numeric)

    xdata = [item[0] for item in xdata.values]
    ydata = [item[0] - 4 for item in ydata.values]

    # print y = x^a +/- b
    z1 = polyfit(xdata, ydata, 2)
    print(z1)

    from scipy import stats
    xdata = stats.zscore(xdata)
    ydata = stats.zscore(ydata)
    z1 = polyfit(xdata, ydata, 2)
    print(z1)

# plot:
def plot(result, picture):
    df = pd.read_csv(result)
    # xdata = df[['inputs']].apply(pd.to_numeric)
    # ydata = df[['time']].apply(pd.to_numeric)
    matplotlib.rcParams.update({'font.size': 30})

    # x轴的label
    xlabel('input size', fontsize=30, labelpad=2)

    # y轴的label
    ylabel(r'time cost(s)', fontsize=30, labelpad=5)
    # df['time'] = df['time'].apply(lambda y: 1000 * y)
    # Create plots with pre-defined labels.
    plt.scatter(df['inputs'], df['time'], marker='o', s=50)

    grid(True)
    xgridlines = getp(gca(), 'xgridlines')
    ygridlines = getp(gca(), 'ygridlines')
    setp(xgridlines, 'linestyle', ':')
    setp(ygridlines, 'linestyle', ':')

    legend = plt.legend(loc='upper center', shadow=False, fontsize='22', numpoints=1)

    # Put a nicer background color on the legend.
    legend.get_frame().set_facecolor('#FFFFFF')

    fig = plt.gcf()

    plt.subplots_adjust(left=0.2, right=0.95, top=0.93, bottom=0.2)
    fig.set_size_inches(8, 8)

    # fig.suptitle('Cost plot (apache34464:WaitForString)', fontsize=10)
    fig.savefig(picture, dpi=300)
    plt.show()

def plot_multi(result, result1, result2, result3,  picture):
    df = pd.read_csv(result)
    df1 = pd.read_csv(result1)
    df2 = pd.read_csv(result2)
    df3 = pd.read_csv(result3)
    # xdata = df[['inputs']].apply(pd.to_numeric)
    # ydata = df[['time']].apply(pd.to_numeric)
    matplotlib.rcParams.update({'font.size': 30})

    # x轴的label
    xlabel('input size', fontsize=30, labelpad=2)

    # y轴的label
    ylabel(r'time cost(s)', fontsize=30, labelpad=5)
    # df['time'] = df['time'].apply(lambda y: 1000 * y)
    # Create plots with pre-defined labels.
    plt.scatter(df['inputs'], df['time'], marker='o', s=50, color='r', label='no pass')
    plt.scatter(df1['inputs'], df1['time'], marker='o', s=50, color='g', label='original pass')
    plt.scatter(df2['inputs'], df2['time'], marker='o', s=50, color='b', label='empty pass')
    plt.scatter(df3['inputs'], df3['time'], marker='o', s=50, color='y', label='clonesample pass')
    plt.legend()

    grid(True)
    xgridlines = getp(gca(), 'xgridlines')
    ygridlines = getp(gca(), 'ygridlines')
    setp(xgridlines, 'linestyle', ':')
    setp(ygridlines, 'linestyle', ':')

    # legend = plt.legend(loc='upper center', shadow=False, fontsize='22', numpoints=1)

    # Put a nicer background color on the legend.
    # legend.get_frame().set_facecolor('#FFFFFF')

    fig = plt.gcf()

    plt.subplots_adjust(left=0.2, right=0.95, top=0.93, bottom=0.2)
    fig.set_size_inches(8, 8)

    # fig.suptitle('Cost plot (apache34464:WaitForString)', fontsize=10)
    fig.savefig(picture, dpi=300)
    plt.show()

if __name__ == '__main__':
	target_O0_dir = './targets.O0/'
	target_O2_dir = './targets.O2/'
	
	result_O0_dir = './results.O0/'
	result_O2_dir = './results.O2/'

	nopass = 'target.nopass'
	original = 'target.lalls'
	clonesample = 'target.clonesample'
	clonesample2 = 'target.clonesample2'
	openshmem = 'target.openshmem'

	run_time_command(target_O0_dir + nopass, result_O0_dir + nopass + '.csv')	
#	run_time_command(target_O2_dir + nopass, result_O2_dir + nopass + '.csv')
#
#	run_time_command(target_O0_dir + original, result_O0_dir + original + '.csv')
#	run_time_command(target_O2_dir + original, result_O2_dir + original + '.csv')
#
	run_time_command(target_O0_dir + clonesample, result_O0_dir + clonesample + '.csv')
#	run_time_command(target_O2_dir + clonesample, result_O2_dir + clonesample + '.csv')
#
#	run_time_command(target_O0_dir + clonesample2, result_O0_dir + clonesample2 + '.csv')
#	run_time_command(target_O2_dir + clonesample2, result_O2_dir + clonesample2 + '.csv')
#
	#run_time_command(target_O0_dir + openshmem, result_O0_dir + openshmem + '.csv')
	#run_time_command(target_O2_dir + openshmem, result_O2_dir + openshmem + '.csv')

	#plot_multi('exe_time.csv', 'exe_time.lalls.csv', 'exe_time.empty.csv', 'exe_time.clonesample.csv', 'time-plot.png')
