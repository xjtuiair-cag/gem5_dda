#! /bin/env python3

import os
import pandas as pd

from string import Template

def extract_stats(record_list, path_template, stats_names):
    filled_record_list = []
    for record in record_list:
        # apply record values to path template
        stats_path = path_template.substitute(record) + '/stats.txt' 
        filled_record = dict(record)

        if not os.path.exists(stats_path):
            for stats_name in stats_names:
                filled_record[stats_name] = 'NAN'
        else: 
            for stats_name in stats_names:
                filled_record[stats_name] = 'EMPTY'
                with open(stats_path, 'r') as stats:
                    for line in stats:
                        # remove comments
                        line = line.split('#')[0].strip()
                        # only for one data per metric-line
                        # only record the first reaching stats_name
                        if stats_name in line:
                            filled_record[stats_name] = line.split(stats_name)[1].strip()
                            break
        filled_record_list.append(filled_record)
    return filled_record_list

def gen_field_record(field_values):
    pre_record_list = []
    cur_record_list = []
    for cur_field_name, cur_field_values in field_values.items():
        for value in cur_field_values:
            if not isinstance(value, str):
                value = str(value)
            if len(pre_record_list) == 0:
                cur_record_list.append({cur_field_name: value})
            else:
                for record in pre_record_list: 
                    new_record = dict(record)
                    new_record[cur_field_name] = value
                    cur_record_list.append(new_record)

        pre_record_list = cur_record_list
        cur_record_list = []
    return pre_record_list


### script config start here ###

stats_names = ['simSeconds', 'system.cpu.committedInsts']

#field_values = {
#        'bench' : ['spmv'], 
#        'matrix' : ['mid'], 
#        'threads' : [4, 6, 8, 12, 16], 
#        'prefetcher' : ['stride', 'dmp'], 
#        'degree' : [4], 
#        'DRAM' : ['DDR4'],
#        }
#path_template = Template('${bench}_${matrix}_TH${threads}/${prefetcher}_DG${degree}_${DRAM}_warm_m5out')

field_values = {
        'bench' : ['spmv'], 
        'matrix' : ['sx-superuser', 'com-Amazon', 'loc-Gowalla', 'msc10848'], 
        'prefetcher' : ['stride'], 
        'degree' : [2, 4, 6, 8], 
        }
path_template = Template('${bench}_${matrix}/${prefetcher}_DG${degree}_m5out')

record_list = gen_field_record(field_values)
filled_record_list = extract_stats(record_list, path_template, stats_names)

stats_data = pd.DataFrame.from_records(filled_record_list)
#print(stats_data)

simSeconds = stats_data.pivot(index='matrix', columns='degree', values='simSeconds')
print(simSeconds)

